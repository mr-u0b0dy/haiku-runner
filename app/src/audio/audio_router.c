#include <errno.h>

#include "audio_router.h"
#include "audio_pipeline.h"

#if defined(CONFIG_HR_UI_AUDIO_CUE)
#include "audio_cue.h"
#endif

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
#if defined(CONFIG_HR_UI_AUDIO_CUE)
  /* The source-change cue owns the output for its duration; mixing live input
   * into it would make the announcement unintelligible. */
  if (audio_cue_active()) {
    return -EBUSY;
  }
#endif

  return audio_pipeline_push(frame);
}
