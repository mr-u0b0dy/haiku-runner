#ifndef HAIKU_RUNNER_INPUT_USB_UAC_H_
#define HAIKU_RUNNER_INPUT_USB_UAC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int input_usb_set_connected(bool connected);
int input_usb_push_pcm_bytes(const uint8_t *data, size_t size);

/* Pops exactly `size` bytes, or returns -EAGAIN if that much isn't buffered
 * yet. Used to re-chunk variable-sized USB frames into a stable frame size. */
int input_usb_take_pcm_bytes(uint8_t *data, size_t size);

/* Brings up the USB device stack and the UAC2 audio function. */
int input_usb_uac2_init(void);
int input_usb_receive_frame(const uint8_t *data,
                              size_t size,
                              uint32_t sample_rate_hz,
                              uint8_t channels,
                              uint8_t bits_per_sample);

#endif
