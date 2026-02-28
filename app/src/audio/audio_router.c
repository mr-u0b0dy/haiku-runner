#include "audio_router.h"
#include "audio_pipeline.h"

int audio_router_init(void)
{
  return audio_pipeline_init();
}

int audio_router_start(void)
{
  return audio_pipeline_start();
}

int audio_router_stop(void)
{
  return audio_pipeline_stop();
}

int audio_router_submit(const struct audio_frame *frame)
{
  return audio_pipeline_push(frame);
}
