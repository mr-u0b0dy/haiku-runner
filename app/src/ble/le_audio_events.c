#include "le_audio_sink.h"
#include "source_manager.h"

void le_audio_on_stream_ready(void)
{
  (void)le_audio_sink_set_link_state(true);
}

void le_audio_on_stream_lost(void)
{
  (void)le_audio_sink_set_link_state(false);
  source_manager_tick();
}
