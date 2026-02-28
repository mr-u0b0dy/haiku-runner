#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "input_registry.h"

LOG_MODULE_REGISTER(input_wifi, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;

static int wifi_init(void) { return 0; }
static int wifi_start(void) { return 0; }
static int wifi_stop(void) { return 0; }
static bool wifi_healthy(void) { return false; }
static int wifi_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  ARG_UNUSED(g_callback);
  return 0;
}

static const struct audio_input_ops g_wifi_ops = {
  .init = wifi_init,
  .start = wifi_start,
  .stop = wifi_stop,
  .healthy = wifi_healthy,
  .set_frame_callback = wifi_set_callback,
};

static const struct audio_input_descriptor g_wifi_descriptor = {
  .id = AUDIO_INPUT_WIFI,
  .name = "wifi",
  .ops = &g_wifi_ops,
};

int input_adapter_wifi_register(void)
{
  LOG_INF("Wi-Fi adapter registered");
  return input_registry_register(&g_wifi_descriptor);
}
