#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "input_registry.h"

LOG_MODULE_REGISTER(le_audio_sink, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;
static bool g_link_up;

static int ble_init(void)
{
  g_link_up = false;
  return 0;
}

static int ble_start(void)
{
  g_link_up = true;
  LOG_INF("BLE sink started");
  return 0;
}

static int ble_stop(void)
{
  g_link_up = false;
  return 0;
}

static bool ble_healthy(void)
{
  return g_link_up;
}

static int ble_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  ARG_UNUSED(g_callback);
  return 0;
}

static const struct audio_input_ops g_ble_ops = {
  .init = ble_init,
  .start = ble_start,
  .stop = ble_stop,
  .healthy = ble_healthy,
  .set_frame_callback = ble_set_callback,
};

static const struct audio_input_descriptor g_ble_descriptor = {
  .id = AUDIO_INPUT_BLE,
  .name = "ble",
  .ops = &g_ble_ops,
};

int input_adapter_ble_register(void)
{
  LOG_INF("BLE input adapter registered");
  return input_registry_register(&g_ble_descriptor);
}
