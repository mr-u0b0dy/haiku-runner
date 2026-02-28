#ifndef HAIKU_RUNNER_INPUT_USB_C_UAC_H_
#define HAIKU_RUNNER_INPUT_USB_C_UAC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int input_usb_c_set_connected(bool connected);
int input_usb_c_receive_frame(const uint8_t *data,
                              size_t size,
                              uint32_t sample_rate_hz,
                              uint8_t channels,
                              uint8_t bits_per_sample);

#endif
