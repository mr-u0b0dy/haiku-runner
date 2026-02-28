#include "source_manager.h"

void le_audio_on_stream_lost(void)
{
  source_manager_tick();
}
