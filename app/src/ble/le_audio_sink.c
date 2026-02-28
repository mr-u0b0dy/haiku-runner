#include <errno.h>
#include <zephyr/logging/log.h>

#include "input_frame_ingress.h"
#include "input_registry.h"
#include "le_audio_sink.h"

LOG_MODULE_REGISTER(le_audio_sink, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;
static bool g_link_up;

static int ble_poll(void)
{
  return 0;
}

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
  return 0;
}

int le_audio_sink_set_link_state(bool up)
{
  g_link_up = up;
  return 0;
}

int le_audio_sink_receive_frame(const uint8_t *data,
                                size_t size,
                                uint32_t sample_rate_hz,
                                uint8_t channels,
                                uint8_t bits_per_sample)
{
  return input_frame_ingress_deliver(g_callback,
                                     g_link_up,
                                     AUDIO_INPUT_BLE,
                                     data,
                                     size,
                                     sample_rate_hz,
                                     channels,
                                     bits_per_sample);
}

static const struct audio_input_ops g_ble_ops = {
  .init = ble_init,
  .start = ble_start,
  .stop = ble_stop,
  .poll = ble_poll,
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
