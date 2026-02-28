#ifndef HAIKU_RUNNER_LE_AUDIO_SINK_H_
#define HAIKU_RUNNER_LE_AUDIO_SINK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int le_audio_sink_set_link_state(bool up);
int le_audio_sink_receive_frame(const uint8_t *data,
                                size_t size,
                                uint32_t sample_rate_hz,
                                uint8_t channels,
                                uint8_t bits_per_sample);

#endif
