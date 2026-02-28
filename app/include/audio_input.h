#ifndef HAIKU_RUNNER_AUDIO_INPUT_H_
#define HAIKU_RUNNER_AUDIO_INPUT_H_

#include <stdbool.h>
#include "audio_frame.h"

enum audio_input_id {
  AUDIO_INPUT_BLE,
  AUDIO_INPUT_AUX,
  AUDIO_INPUT_USB_C,
  AUDIO_INPUT_WIFI,
  AUDIO_INPUT_UNKNOWN,
};

typedef void (*audio_input_frame_callback_t)(enum audio_input_id id, const struct audio_frame *frame);

struct audio_input_ops {
  int (*init)(void);
  int (*start)(void);
  int (*stop)(void);
  bool (*healthy)(void);
  int (*set_frame_callback)(audio_input_frame_callback_t callback);
};

struct audio_input_descriptor {
  enum audio_input_id id;
  const char *name;
  const struct audio_input_ops *ops;
};

#endif
