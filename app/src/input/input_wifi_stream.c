#include <errno.h>
#include <zephyr/logging/log.h>

#include "input_registry.h"
#include "input_wifi_stream.h"

LOG_MODULE_REGISTER(input_wifi, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;
static bool g_connected;

static int wifi_init(void)
{
  g_connected = false;
  return 0;
}
static int wifi_start(void) { return 0; }
static int wifi_stop(void) { return 0; }
static bool wifi_healthy(void) { return g_connected; }
static int wifi_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  return 0;
}

int input_wifi_set_connected(bool connected)
{
  g_connected = connected;
  return 0;
}

int input_wifi_receive_frame(const uint8_t *data,
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

  g_callback(AUDIO_INPUT_WIFI, &frame);
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
