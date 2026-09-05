/* Audible source-change cues.
 *
 * When the active input changes the speaker announces which one it is, so the
 * device is usable without a console attached. The cue is encoded twice over
 * for clarity: each source has both a distinct number of beeps and a distinct
 * pitch, so it is identifiable even if the first beep is missed.
 */
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "audio_backend.h"
#include "audio_cue.h"

LOG_MODULE_REGISTER(audio_cue, LOG_LEVEL_INF);

#define CUE_SAMPLE_RATE_HZ 48000U
#define CUE_FRAME_SAMPLES  480U /* 10 ms, matching the I2S block geometry */
#define CUE_BEEP_MS        130U
#define CUE_GAP_MS          90U

/* Peak amplitude. Kept well below full scale: this is a notification, and it
 * may land on top of a listener already at a comfortable volume. */
#define CUE_AMPLITUDE 9000

/* Prime the I2S queue before pacing, since the backend only starts its
 * transmitter once a few blocks are buffered. */
#define CUE_PRIME_FRAMES 3U

static int16_t g_cue_frame[CUE_FRAME_SAMPLES];
static atomic_t g_cue_active;

/* Quarter-wave sine, 64 entries, Q15-ish - enough resolution for a beep and
 * far cheaper than pulling in floating point. */
static const int16_t sine_quarter[65] = {
      0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
   6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
  12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
  18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
  23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
  27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
  30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
  32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
  32767,
};

static int16_t cue_sine(uint32_t phase)
{
  /* phase is 0..255 across one full cycle, folded onto the quarter table. */
  uint32_t quadrant = (phase >> 6) & 0x3U;
  uint32_t index = phase & 0x3FU;

  switch (quadrant) {
  case 0:
    return sine_quarter[index];
  case 1:
    return sine_quarter[64U - index];
  case 2:
    return (int16_t)-sine_quarter[index];
  default:
    return (int16_t)-sine_quarter[64U - index];
  }
}

/* Emits `ms` milliseconds of either a tone or silence, paced so the backend's
 * buffer neither starves nor overflows. */
static void cue_emit(uint32_t freq_hz, uint32_t ms, uint32_t *dds_phase, uint32_t *frame_count)
{
  const uint32_t frames = (ms * CUE_SAMPLE_RATE_HZ) / (CUE_FRAME_SAMPLES * 1000U);
  const uint32_t step = freq_hz > 0U
                          ? (uint32_t)(((uint64_t)freq_hz << 24) / CUE_SAMPLE_RATE_HZ)
                          : 0U;

  for (uint32_t f = 0; f < frames; ++f) {
    for (uint32_t i = 0; i < CUE_FRAME_SAMPLES; ++i) {
      if (freq_hz == 0U) {
        g_cue_frame[i] = 0;
        continue;
      }

      int32_t sample = cue_sine(*dds_phase >> 16) * (int32_t)CUE_AMPLITUDE;

      g_cue_frame[i] = (int16_t)(sample / INT16_MAX);
      *dds_phase += step;
    }

    struct audio_frame frame = {
      .data = (const uint8_t *)g_cue_frame,
      .size = sizeof(g_cue_frame),
      .sample_rate_hz = CUE_SAMPLE_RATE_HZ,
      .channels = 1U,
      .bits_per_sample = 16U,
    };

    (void)audio_backend_write(&frame);

    /* Queue a few blocks before pacing, then track real time so the
     * transmitter is neither starved nor pushed into backpressure. */
    if (++(*frame_count) > CUE_PRIME_FRAMES) {
      k_sleep(K_MSEC((CUE_FRAME_SAMPLES * 1000U) / CUE_SAMPLE_RATE_HZ));
    }
  }
}

/* Beep count and pitch per source. Both differ so a cue is recognisable
 * either by counting or by ear. */
static void cue_pattern_for(enum audio_input_id id, uint8_t *beeps, uint32_t *freq_hz)
{
  switch (id) {
  case AUDIO_INPUT_BLE:
    *beeps = 1U;
    *freq_hz = 1047U; /* C6 */
    break;
  case AUDIO_INPUT_USB:
    *beeps = 2U;
    *freq_hz = 880U; /* A5 */
    break;
  case AUDIO_INPUT_AUX:
    *beeps = 3U;
    *freq_hz = 659U; /* E5 */
    break;
  default:
    *beeps = 4U;
    *freq_hz = 440U; /* A4 - unknown/no source */
    break;
  }
}

void audio_cue_play_source(enum audio_input_id id)
{
  uint8_t beeps;
  uint32_t freq_hz;

  cue_pattern_for(id, &beeps, &freq_hz);

  LOG_INF("cue: source %d (%u beeps at %u Hz)", id, beeps, freq_hz);

  atomic_set(&g_cue_active, 1);

  uint32_t dds_phase = 0U;
  uint32_t frame_count = 0U;

  for (uint8_t i = 0; i < beeps; ++i) {
    cue_emit(freq_hz, CUE_BEEP_MS, &dds_phase, &frame_count);
    cue_emit(0U, CUE_GAP_MS, &dds_phase, &frame_count);
  }

  atomic_set(&g_cue_active, 0);
}

bool audio_cue_active(void)
{
  return atomic_get(&g_cue_active) != 0;
}
