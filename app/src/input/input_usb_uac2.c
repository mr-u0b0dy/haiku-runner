/* USB Audio Class 2 playback device backing the USB input adapter.
 *
 * The board enumerates as a USB sound card; a host (phone in OTG mode, or a
 * PC) streams PCM to it with no drivers, no app and no pairing. Received
 * audio is handed to input_usb_c_receive_frame(), i.e. the same ingress path
 * every other adapter uses, so source arbitration and the I2S backend are
 * reused unchanged.
 */
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_uac2.h>

#include "input_usb_c_uac.h"

LOG_MODULE_REGISTER(input_usb_uac2, LOG_LEVEL_INF);

#define HR_UAC2_SAMPLE_RATE_HZ 48000U
#define HR_UAC2_CHANNELS       2U
#define HR_UAC2_BITS_PER_SAMPLE 16U
#define HR_UAC2_BYTES_PER_FRAME (HR_UAC2_CHANNELS * (HR_UAC2_BITS_PER_SAMPLE / 8U))

/* Deliver 10 ms chunks downstream. USB hands us one (micro)frame at a time -
 * ~48 samples, and one more or less as the host's clock drifts - whereas the
 * I2S backend wants a stable block size (it reconfigures the peripheral when
 * the frame geometry changes). Re-chunking through the adapter's ring buffer
 * absorbs that jitter and keeps the I2S side steady. */
#define HR_UAC2_CHUNK_SAMPLES 480U
#define HR_UAC2_CHUNK_BYTES (HR_UAC2_CHUNK_SAMPLES * HR_UAC2_BYTES_PER_FRAME)

/* One USB frame at 48 kHz stereo 16-bit is 192 bytes; leave generous room for
 * the host sending a sample more, and for high-speed microframe sizing. */
#define HR_UAC2_RX_BLOCK_SIZE 512U
#define HR_UAC2_RX_BLOCK_COUNT 6U

K_MEM_SLAB_DEFINE_STATIC(g_uac2_rx_slab, HR_UAC2_RX_BLOCK_SIZE, HR_UAC2_RX_BLOCK_COUNT, 4);

/* Zephyr project vendor ID: for development only, not for shipping product. */
#define HR_USB_VID 0x2fe3
#define HR_USB_PID 0x0011

USBD_DEVICE_DEFINE(g_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), HR_USB_VID, HR_USB_PID);

USBD_DESC_LANG_DEFINE(g_usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(g_usbd_mfr, "haiku-runner");
USBD_DESC_PRODUCT_DEFINE(g_usbd_product, "Haiku Runner Speaker");
USBD_DESC_CONFIG_DEFINE(g_usbd_fs_cfg_desc, "FS Configuration");

/* Bus powered: in OTG the host (phone) supplies VBUS. */
USBD_CONFIGURATION_DEFINE(g_usbd_fs_config, 0, 250, &g_usbd_fs_cfg_desc);

static const struct device *uac2_dev(void)
{
  return DEVICE_DT_GET(DT_NODELABEL(uac2_speaker));
}

static void uac2_terminal_update_cb(const struct device *dev,
                                    uint8_t terminal,
                                    bool enabled,
                                    bool microframes,
                                    void *user_data)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(terminal);
  ARG_UNUSED(microframes);
  ARG_UNUSED(user_data);

  LOG_INF("USB audio terminal %s", enabled ? "enabled" : "disabled");
  (void)input_usb_c_set_connected(enabled);
}

static void *uac2_get_recv_buf(const struct device *dev,
                               uint8_t terminal,
                               uint16_t size,
                               void *user_data)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(terminal);
  ARG_UNUSED(user_data);

  if (size > HR_UAC2_RX_BLOCK_SIZE) {
    LOG_WRN("USB requested %u byte buffer, block is %u", size, HR_UAC2_RX_BLOCK_SIZE);
    return NULL;
  }

  void *buf;

  if (k_mem_slab_alloc(&g_uac2_rx_slab, &buf, K_NO_WAIT) != 0) {
    return NULL;
  }

  return buf;
}

static void uac2_data_recv_cb(const struct device *dev,
                              uint8_t terminal,
                              void *buf,
                              uint16_t size,
                              void *user_data)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(terminal);
  ARG_UNUSED(user_data);

  if (size > 0U) {
    /* Buffering here rather than forwarding the raw USB frame keeps a stable
     * chunk size downstream; see HR_UAC2_CHUNK_BYTES. */
    if (input_usb_c_push_pcm_bytes(buf, size) != 0) {
      /* Host is running faster than the I2S clock drains us. Dropping is the
       * right failure here: the alternative is unbounded latency growth. */
      LOG_DBG("USB PCM ring full, dropping %u bytes", size);
    }
  }

  k_mem_slab_free(&g_uac2_rx_slab, buf);

  static uint8_t chunk[HR_UAC2_CHUNK_BYTES];

  while (input_usb_c_take_pcm_bytes(chunk, sizeof(chunk)) == 0) {
    (void)input_usb_c_receive_frame(chunk, sizeof(chunk), HR_UAC2_SAMPLE_RATE_HZ,
                                    HR_UAC2_CHANNELS, HR_UAC2_BITS_PER_SAMPLE);
  }
}

static void uac2_buf_release_cb(const struct device *dev,
                                uint8_t terminal,
                                void *buf,
                                void *user_data)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(terminal);
  ARG_UNUSED(user_data);

  k_mem_slab_free(&g_uac2_rx_slab, buf);
}

/* Called every USB Start-of-Frame (1 kHz at full speed). Mandatory: the class
 * invokes it unconditionally, and the __ASSERT guarding it compiles out in
 * release builds, so leaving it NULL faults on the first frame. */
static void uac2_sof_cb(const struct device *dev, void *user_data)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(user_data);
}

/* Explicit feedback, Q10.14 at full speed: how many samples per frame we want
 * the host to send. Nominal is exactly 48 for 48 kHz; +/-1 LSB shifts one
 * sample per 16384 frames.
 *
 * This is a fixed nominal value, i.e. no drift regulation yet. The host's SOF
 * clock and our I2S clock are independent, so they will slowly diverge and
 * the buffer will creep until a block is dropped or the DAC underruns. See
 * Zephyr's uac2_explicit_feedback sample for a real regulator. */
#define HR_UAC2_FEEDBACK_NOMINAL ((HR_UAC2_SAMPLE_RATE_HZ / 1000U) << 14)

static uint32_t uac2_feedback_cb(const struct device *dev,
                                 uint8_t terminal,
                                 void *user_data)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(terminal);
  ARG_UNUSED(user_data);

  return HR_UAC2_FEEDBACK_NOMINAL;
}

static struct uac2_ops g_uac2_ops = {
  .sof_cb = uac2_sof_cb,
  .terminal_update_cb = uac2_terminal_update_cb,
  .get_recv_buf = uac2_get_recv_buf,
  .data_recv_cb = uac2_data_recv_cb,
  .buf_release_cb = uac2_buf_release_cb,
  .feedback_cb = uac2_feedback_cb,
};

int input_usb_c_uac2_init(void)
{
  int err;

  usbd_uac2_set_ops(uac2_dev(), &g_uac2_ops, NULL);

  err = usbd_add_descriptor(&g_usbd, &g_usbd_lang);
  if (err != 0) {
    LOG_ERR("usbd_add_descriptor(lang) failed: %d", err);
    return err;
  }

  err = usbd_add_descriptor(&g_usbd, &g_usbd_mfr);
  if (err != 0) {
    LOG_ERR("usbd_add_descriptor(manufacturer) failed: %d", err);
    return err;
  }

  err = usbd_add_descriptor(&g_usbd, &g_usbd_product);
  if (err != 0) {
    LOG_ERR("usbd_add_descriptor(product) failed: %d", err);
    return err;
  }

  err = usbd_add_configuration(&g_usbd, USBD_SPEED_FS, &g_usbd_fs_config);
  if (err != 0) {
    LOG_ERR("usbd_add_configuration failed: %d", err);
    return err;
  }

  err = usbd_register_all_classes(&g_usbd, USBD_SPEED_FS, 1, NULL);
  if (err != 0) {
    LOG_ERR("usbd_register_all_classes failed: %d", err);
    return err;
  }

  err = usbd_init(&g_usbd);
  if (err != 0) {
    LOG_ERR("usbd_init failed: %d", err);
    return err;
  }

  err = usbd_enable(&g_usbd);
  if (err != 0) {
    LOG_ERR("usbd_enable failed: %d", err);
    return err;
  }

  LOG_INF("USB Audio Class 2 speaker enabled (%u Hz, %u ch, %u-bit)",
          HR_UAC2_SAMPLE_RATE_HZ, HR_UAC2_CHANNELS, HR_UAC2_BITS_PER_SAMPLE);
  return 0;
}
