#ifndef HAIKU_RUNNER_INPUT_FRAME_INGRESS_H_
#define HAIKU_RUNNER_INPUT_FRAME_INGRESS_H_

#include <stddef.h>
#include <stdint.h>

#include "audio_input.h"

int input_frame_ingress_deliver(audio_input_frame_callback_t callback,
                                bool source_ready,
                                enum audio_input_id id,
                                const uint8_t *data,
                                size_t size,
                                uint32_t sample_rate_hz,
                                uint8_t channels,
                                uint8_t bits_per_sample);

#endif
