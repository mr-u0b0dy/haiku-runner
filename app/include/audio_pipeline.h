#ifndef HAIKU_RUNNER_AUDIO_PIPELINE_H_
#define HAIKU_RUNNER_AUDIO_PIPELINE_H_

#include "audio_frame.h"

int audio_pipeline_init(void);
int audio_pipeline_start(void);
int audio_pipeline_stop(void);
int audio_pipeline_push(const struct audio_frame *frame);

#endif
