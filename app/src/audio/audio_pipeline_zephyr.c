#include "audio_backend.h"
#include "audio_pipeline.h"

int audio_pipeline_init(void)
{
  return audio_backend_init();
}

int audio_pipeline_start(void)
{
  return audio_backend_start();
}

int audio_pipeline_stop(void)
{
  return audio_backend_stop();
}

int audio_pipeline_push(const struct audio_frame *frame)
{
  return audio_backend_write(frame);
}
