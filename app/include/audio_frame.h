#ifndef HAIKU_RUNNER_AUDIO_FRAME_H_
#define HAIKU_RUNNER_AUDIO_FRAME_H_

#include <stddef.h>
#include <stdint.h>

struct audio_frame {
  const uint8_t *data;
  size_t size;
  uint32_t sample_rate_hz;
  uint8_t channels;
  uint8_t bits_per_sample;
};

#endif
