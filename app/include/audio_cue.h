#ifndef HAIKU_RUNNER_AUDIO_CUE_H_
#define HAIKU_RUNNER_AUDIO_CUE_H_

#include <stdbool.h>

#include "audio_input.h"

/* Plays a short beep pattern identifying a source, straight to the output
 * backend. Deliberately bypasses source_manager: the cue has to be heard
 * whichever input is active, including none.
 *
 * Blocks for the length of the cue (a few hundred ms), so call it from a
 * thread that can sleep - not from an ISR or the system workqueue.
 */
void audio_cue_play_source(enum audio_input_id id);

/* True while a cue is sounding. audio_router drops input frames during that
 * window so the cue is not interleaved with live audio. */
bool audio_cue_active(void);

#endif
