#include <errno.h>
#include <zephyr/sys/util.h>

#include "input_registry.h"

static const struct audio_input_descriptor *g_inputs[CONFIG_HR_SOURCE_MANAGER_MAX_INPUTS];
static size_t g_input_count;

int input_registry_register(const struct audio_input_descriptor *descriptor)
{
  if (descriptor == NULL || descriptor->ops == NULL) {
    return -EINVAL;
  }

  if (g_input_count >= ARRAY_SIZE(g_inputs)) {
    return -ENOMEM;
  }

  g_inputs[g_input_count++] = descriptor;
  return 0;
}

const struct audio_input_descriptor *input_registry_get(enum audio_input_id id)
{
  for (size_t index = 0; index < g_input_count; ++index) {
    if (g_inputs[index]->id == id) {
      return g_inputs[index];
    }
  }

  return NULL;
}

size_t input_registry_count(void)
{
  return g_input_count;
}

const struct audio_input_descriptor *input_registry_at(size_t index)
{
  if (index >= g_input_count) {
    return NULL;
  }

  return g_inputs[index];
}
