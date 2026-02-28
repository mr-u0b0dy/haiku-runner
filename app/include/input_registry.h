#ifndef HAIKU_RUNNER_INPUT_REGISTRY_H_
#define HAIKU_RUNNER_INPUT_REGISTRY_H_

#include <stddef.h>
#include "audio_input.h"

int input_registry_register(const struct audio_input_descriptor *descriptor);
const struct audio_input_descriptor *input_registry_get(enum audio_input_id id);
size_t input_registry_count(void);
const struct audio_input_descriptor *input_registry_at(size_t index);

#endif
