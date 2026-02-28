#include <errno.h>

#include "input_frame_ingress.h"

int input_frame_ingress_deliver(audio_input_frame_callback_t callback,
                                bool source_ready,
                                enum audio_input_id id,
                                const uint8_t *data,
                                size_t size,
                                uint32_t sample_rate_hz,
                                uint8_t channels,
                                uint8_t bits_per_sample)
{
  if (!source_ready || callback == NULL || data == NULL || size == 0U) {
    return -EINVAL;
  }

  struct audio_frame frame = {
    .data = data,
    .size = size,
    .sample_rate_hz = sample_rate_hz,
    .channels = channels,
    .bits_per_sample = bits_per_sample,
  };

  callback(id, &frame);
  return 0;
}
