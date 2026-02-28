#ifndef HAIKU_RUNNER_AUDIO_BACKEND_H_
#define HAIKU_RUNNER_AUDIO_BACKEND_H_

#include "audio_frame.h"

int audio_backend_init(void);
int audio_backend_start(void);
int audio_backend_stop(void);
int audio_backend_write(const struct audio_frame *frame);

#endif
