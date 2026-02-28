#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "audio_backend.h"

LOG_MODULE_REGISTER(audio_backend, LOG_LEVEL_INF);

int audio_backend_init(void)
{
  LOG_INF("audio backend init (stub)");
  return 0;
}

int audio_backend_start(void)
{
  LOG_INF("audio backend start (stub)");
  return 0;
}

int audio_backend_stop(void)
{
  LOG_INF("audio backend stop (stub)");
  return 0;
}

int audio_backend_write(const struct audio_frame *frame)
{
  ARG_UNUSED(frame);
  return 0;
}
