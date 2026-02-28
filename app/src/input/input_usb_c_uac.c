#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "input_registry.h"

LOG_MODULE_REGISTER(input_usb_c, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;

static int usb_init(void) { return 0; }
static int usb_start(void) { return 0; }
static int usb_stop(void) { return 0; }
static bool usb_healthy(void) { return false; }
static int usb_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  ARG_UNUSED(g_callback);
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
