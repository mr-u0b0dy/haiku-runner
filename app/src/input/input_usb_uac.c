#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "input_frame_ingress.h"
#include "input_registry.h"
#include "input_usb_uac.h"

LOG_MODULE_REGISTER(input_usb, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;
static bool g_connected;
RING_BUF_DECLARE(g_usb_pcm_ring, CONFIG_HR_INPUT_USB_RING_BUFFER_BYTES);

static int usb_poll(void)
{
#if defined(CONFIG_HR_INPUT_USB_UAC2)
  /* The UAC2 callback owns draining the ring: it delivers fixed-size chunks
   * straight from USB context. Draining here as well would mean two
   * consumers pulling different sized chunks from one buffer, which makes
   * the frame geometry flip-flop and forces the I2S backend to reconfigure
   * on nearly every write. */
  return 0;
#else
  if (!g_connected || g_callback == NULL) {
    return 0;
  }

  uint8_t frame_data[CONFIG_HR_INPUT_USB_FRAME_BYTES];
  uint32_t read_size = ring_buf_get(&g_usb_pcm_ring,
                                    frame_data,
                                    sizeof(frame_data));

  if (read_size == 0U) {
    return 0;
  }

  return input_usb_receive_frame(frame_data,
                                   read_size,
                                   CONFIG_HR_INPUT_USB_SAMPLE_RATE_HZ,
                                   CONFIG_HR_INPUT_USB_CHANNELS,
                                   CONFIG_HR_INPUT_USB_BITS_PER_SAMPLE);
#endif
}

static int usb_init(void)
{
  g_connected = false;
  ring_buf_reset(&g_usb_pcm_ring);

#if defined(CONFIG_HR_INPUT_USB_UAC2)
  return input_usb_uac2_init();
#else
  return 0;
#endif
}
static int usb_start(void) { return 0; }
static int usb_stop(void)
{
  ring_buf_reset(&g_usb_pcm_ring);
  return 0;
}
static bool usb_healthy(void) { return g_connected; }
static int usb_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  return 0;
}

int input_usb_set_connected(bool connected)
{
  g_connected = connected;
  if (!connected) {
    ring_buf_reset(&g_usb_pcm_ring);
  }
  return 0;
}

int input_usb_push_pcm_bytes(const uint8_t *data, size_t size)
{
  if (data == NULL || size == 0U) {
    return -EINVAL;
  }

  /* All-or-nothing: ring_buf_put() would otherwise write only what fits,
   * and a partial write leaves the buffer offset mid-sample, permanently
   * shifting channel alignment for everything that follows. */
  if (ring_buf_space_get(&g_usb_pcm_ring) < size) {
    return -ENOSPC;
  }

  uint32_t written = ring_buf_put(&g_usb_pcm_ring, data, size);

  if (written != size) {
    return -ENOSPC;
  }

  return 0;
}

int input_usb_take_pcm_bytes(uint8_t *data, size_t size)
{
  if (data == NULL || size == 0U) {
    return -EINVAL;
  }

  if (ring_buf_size_get(&g_usb_pcm_ring) < size) {
    return -EAGAIN;
  }

  return ring_buf_get(&g_usb_pcm_ring, data, size) == size ? 0 : -EAGAIN;
}

int input_usb_receive_frame(const uint8_t *data,
                              size_t size,
                              uint32_t sample_rate_hz,
                              uint8_t channels,
                              uint8_t bits_per_sample)
{
  return input_frame_ingress_deliver(g_callback,
                                     g_connected,
                                     AUDIO_INPUT_USB,
                                     data,
                                     size,
                                     sample_rate_hz,
                                     channels,
                                     bits_per_sample);
}

static const struct audio_input_ops g_usb_ops = {
  .init = usb_init,
  .start = usb_start,
  .stop = usb_stop,
  .poll = usb_poll,
  .healthy = usb_healthy,
  .set_frame_callback = usb_set_callback,
};

static const struct audio_input_descriptor g_usb_descriptor = {
  .id = AUDIO_INPUT_USB,
  .name = "usb",
  .ops = &g_usb_ops,
};

int input_adapter_usb_register(void)
{
  LOG_INF("USB audio adapter registered");
  return input_registry_register(&g_usb_descriptor);
}
