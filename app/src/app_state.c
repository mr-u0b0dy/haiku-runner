#include "source_manager.h"

enum audio_input_id app_state_current_source(void)
{
  return source_manager_active();
}
