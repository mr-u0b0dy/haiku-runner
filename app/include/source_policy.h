#ifndef HAIKU_RUNNER_SOURCE_POLICY_H_
#define HAIKU_RUNNER_SOURCE_POLICY_H_

#include <stdbool.h>
#include "audio_input.h"

enum source_mode {
  SOURCE_MODE_MANUAL,
  SOURCE_MODE_HYBRID,
};

struct source_policy {
  enum source_mode mode;
  enum audio_input_id manual_selection;
  bool allow_auto_fallback;
};

#endif
