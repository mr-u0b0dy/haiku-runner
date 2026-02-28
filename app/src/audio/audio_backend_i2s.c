#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "audio_backend.h"

LOG_MODULE_REGISTER(audio_backend_i2s, LOG_LEVEL_INF);

int audio_backend_init(void)
{
  LOG_INF("audio backend init (i2s placeholder)");
  return 0;
}

int audio_backend_start(void)
{
  LOG_INF("audio backend start (i2s placeholder)");
  return 0;
}

int audio_backend_stop(void)
{
  LOG_INF("audio backend stop (i2s placeholder)");
  return 0;
}

int audio_backend_write(const struct audio_frame *frame)
{
  ARG_UNUSED(frame);
  return 0;
}
