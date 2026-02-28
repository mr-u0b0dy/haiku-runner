#include <errno.h>
#include <zephyr/logging/log.h>

#include "input_registry.h"
#include "input_usb_c_uac.h"

LOG_MODULE_REGISTER(input_usb_c, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;
static bool g_connected;

static int usb_init(void)
{
  g_connected = false;
  return 0;
}
static int usb_start(void) { return 0; }
static int usb_stop(void) { return 0; }
static bool usb_healthy(void) { return g_connected; }
static int usb_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  return 0;
}

int input_usb_c_set_connected(bool connected)
{
  g_connected = connected;
  return 0;
}

int input_usb_c_receive_frame(const uint8_t *data,
                              size_t size,
                              uint32_t sample_rate_hz,
                              uint8_t channels,
                              uint8_t bits_per_sample)
{
  if (!g_connected || g_callback == NULL || data == NULL || size == 0U) {
    return -EINVAL;
  }

  struct audio_frame frame = {
    .data = data,
    .size = size,
    .sample_rate_hz = sample_rate_hz,
    .channels = channels,
    .bits_per_sample = bits_per_sample,
  };

  g_callback(AUDIO_INPUT_USB_C, &frame);
  return 0;
}

static const struct audio_input_ops g_usb_ops = {
  .init = usb_init,
  .start = usb_start,
  .stop = usb_stop,
  .healthy = usb_healthy,
  .set_frame_callback = usb_set_callback,
};

static const struct audio_input_descriptor g_usb_descriptor = {
  .id = AUDIO_INPUT_USB_C,
  .name = "usb_c",
  .ops = &g_usb_ops,
};

int input_adapter_usb_c_register(void)
{
  LOG_INF("USB-C adapter registered");
  return input_registry_register(&g_usb_descriptor);
}
