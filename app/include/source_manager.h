#ifndef HAIKU_RUNNER_SOURCE_MANAGER_H_
#define HAIKU_RUNNER_SOURCE_MANAGER_H_

#include "audio_input.h"
#include "source_policy.h"

int source_manager_init(const struct source_policy *policy);
int source_manager_start(void);
int source_manager_stop(void);
int source_manager_select(enum audio_input_id id);
enum audio_input_id source_manager_active(void);
void source_manager_on_frame(enum audio_input_id id, const struct audio_frame *frame);
void source_manager_tick(void);

#endif
