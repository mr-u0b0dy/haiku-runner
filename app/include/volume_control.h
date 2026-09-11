#ifndef HAIKU_RUNNER_VOLUME_CONTROL_H_
#define HAIKU_RUNNER_VOLUME_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

#include "audio_frame.h"

/* Digital gain stage shared by every input, so one volume setting covers
 * whichever source is active. Scale matches the Bluetooth VCS Volume_Setting
 * field: 0 is silent, 255 is unity gain. */

void volume_control_set(uint8_t volume);
uint8_t volume_control_get(void);

void volume_control_set_mute(bool mute);
bool volume_control_get_mute(void);

/* Scales frame's PCM data in place toward the current volume/mute state. A
 * no-op for anything but 16-bit PCM, so a frame this can't interpret still
 * reaches the backend unmodified rather than being dropped. */
void volume_control_apply(const struct audio_frame *frame);

#endif
