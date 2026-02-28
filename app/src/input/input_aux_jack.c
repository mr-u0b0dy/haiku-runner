#include <errno.h>
#include <zephyr/logging/log.h>

#include "input_registry.h"
#include "input_aux_jack.h"

LOG_MODULE_REGISTER(input_aux_jack, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;
static bool g_plugged;

static int aux_init(void)
{
  g_plugged = false;
  return 0;
}

static int aux_start(void) { return 0; }
static int aux_stop(void) { return 0; }
static bool aux_healthy(void) { return g_plugged; }
static int aux_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  return 0;
}

int input_aux_set_plugged(bool plugged)
{
  g_plugged = plugged;
  return 0;
}

int input_aux_receive_frame(const uint8_t *data,
                            size_t size,
                            uint32_t sample_rate_hz,
                            uint8_t channels,
                            uint8_t bits_per_sample)
{
  if (!g_plugged || g_callback == NULL || data == NULL || size == 0U) {
    return -EINVAL;
  }

  struct audio_frame frame = {
    .data = data,
    .size = size,
    .sample_rate_hz = sample_rate_hz,
    .channels = channels,
    .bits_per_sample = bits_per_sample,
  };

  g_callback(AUDIO_INPUT_AUX, &frame);
  return 0;
}

static const struct audio_input_ops g_aux_ops = {
  .init = aux_init,
  .start = aux_start,
  .stop = aux_stop,
  .healthy = aux_healthy,
  .set_frame_callback = aux_set_callback,
};

static const struct audio_input_descriptor g_aux_descriptor = {
  .id = AUDIO_INPUT_AUX,
  .name = "aux",
  .ops = &g_aux_ops,
};

int input_adapter_aux_register(void)
{
  LOG_INF("AUX adapter registered");
  return input_registry_register(&g_aux_descriptor);
}
