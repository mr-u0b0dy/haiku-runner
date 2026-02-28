#ifndef HAIKU_RUNNER_AUDIO_ROUTER_H_
#define HAIKU_RUNNER_AUDIO_ROUTER_H_

#include "audio_frame.h"

int audio_router_init(void);
int audio_router_start(void);
int audio_router_stop(void);
int audio_router_submit(const struct audio_frame *frame);

#endif
