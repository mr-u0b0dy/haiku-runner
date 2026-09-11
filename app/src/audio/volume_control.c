/* Digital volume stage, applied once in audio_router regardless of which
 * input is active. It exists because output hardware here has fixed analog
 * gain (the MAX98357A), so "volume" can only ever mean scaling the PCM.
 */
#include <string.h>

#include <zephyr/sys/atomic.h>

#include "volume_control.h"

/* Set from Bluetooth callbacks (the BT RX thread, via VCS) and read from
 * whichever input's delivery thread is currently active; atomics keep that
 * handoff race-free without a mutex on the audio-rate path.
 *
 * Defaults to unity: this gain applies to every source, not just BLE, so
 * booting anywhere below full scale would quietly attenuate USB/AUX too
 * whenever BLE happens to be compiled in, before any phone has said
 * anything about volume at all. */
static atomic_t g_volume = ATOMIC_INIT(255);
static atomic_t g_mute;

/* Above this fraction of full scale, samples are soft-limited rather than
 * passed straight through: MAX98357A output headroom depends on VIN (2.5 V
 * gives noticeably less clean swing than 5 V - see
 * docs-site/content/test-hardware/max98357a-wiring.md), and hot source
 * material at or near 0 dBFS will hard-clip against whatever headroom
 * actually exists on a given build. Rounding the top off in the digital
 * domain trades a little perceived loudness for not slamming a squared-off
 * waveform into the amp. 26214 is 80% of INT16_MAX. */
#define LIMITER_KNEE 26214

/* Smoothly compresses magnitudes above LIMITER_KNEE into
 * [LIMITER_KNEE, INT16_MAX] instead of letting them hit a hard ceiling:
 * mag' = knee + range * over / (over + range), which approaches but never
 * reaches INT16_MAX as `over` grows. Below the knee this is a no-op, so it
 * costs nothing for normal-level material and only engages for peaks that
 * were already at or beyond the edge of full scale. */
static int16_t soft_limit(int32_t sample)
{
  const int32_t sign = sample < 0 ? -1 : 1;
  const int32_t mag = sign * sample;

  if (mag <= LIMITER_KNEE) {
    return (int16_t)sample;
  }

  const int32_t range = INT16_MAX - LIMITER_KNEE;
  const int32_t over = mag - LIMITER_KNEE;
  int32_t limited = LIMITER_KNEE + (range * over) / (over + range);

  if (limited > INT16_MAX) {
    limited = INT16_MAX;
  }

  return (int16_t)(sign * limited);
}

void volume_control_set(uint8_t volume)
{
  atomic_set(&g_volume, volume);
}

uint8_t volume_control_get(void)
{
  return (uint8_t)atomic_get(&g_volume);
}

void volume_control_set_mute(bool mute)
{
  atomic_set(&g_mute, mute ? 1 : 0);
}

bool volume_control_get_mute(void)
{
  return atomic_get(&g_mute) != 0;
}

void volume_control_apply(const struct audio_frame *frame)
{
  if (frame == NULL || frame->data == NULL || frame->bits_per_sample != 16U) {
    return;
  }

  /* Every producer (BT RX, USB, AUX) delivers a frame synchronously and does
   * not touch its buffer again afterward, so scaling in place avoids a copy
   * on the audio-rate path. */
  int16_t *samples = (int16_t *)(void *)frame->data;
  const size_t count = frame->size / sizeof(int16_t);

  if (volume_control_get_mute()) {
    memset(samples, 0, count * sizeof(int16_t));
    return;
  }

  const uint32_t volume = volume_control_get();

  for (size_t i = 0; i < count; ++i) {
    int32_t scaled = samples[i];

    if (volume < 255U) {
      scaled = (scaled * (int32_t)volume) / 255;
    }

    samples[i] = soft_limit(scaled);
  }
}
