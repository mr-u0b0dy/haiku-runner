#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "input_registry.h"

LOG_MODULE_REGISTER(input_aux_jack, LOG_LEVEL_INF);

static audio_input_frame_callback_t g_callback;

static int aux_init(void) { return 0; }
static int aux_start(void) { return 0; }
static int aux_stop(void) { return 0; }
static bool aux_healthy(void) { return false; }
static int aux_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  ARG_UNUSED(g_callback);
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
