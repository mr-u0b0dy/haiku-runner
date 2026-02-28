#ifndef HAIKU_RUNNER_INPUT_AUX_JACK_H_
#define HAIKU_RUNNER_INPUT_AUX_JACK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int input_aux_set_plugged(bool plugged);
int input_aux_receive_frame(const uint8_t *data,
                            size_t size,
                            uint32_t sample_rate_hz,
                            uint8_t channels,
                            uint8_t bits_per_sample);

#endif
