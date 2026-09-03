#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "audio_backend.h"

LOG_MODULE_REGISTER(audio_backend_i2s, LOG_LEVEL_INF);

int audio_backend_write(const struct audio_frame *frame)
{
  ARG_UNUSED(frame);
  return 0;
}

#if defined(CONFIG_HR_BACKEND_I2S_TEST_TONE)

#include <zephyr/drivers/i2s.h>

#define HR_I2S_SAMPLE_RATE_HZ 48000U
#define HR_I2S_CHANNELS       2U
#define HR_I2S_WORD_SIZE_BITS 16U

#define AUDIO_BLOCK_SAMPLES 960U /* 20 ms per block at 48 kHz */
#define AUDIO_BLOCK_COUNT   4U
#define AUDIO_BLOCK_SIZE (AUDIO_BLOCK_SAMPLES * HR_I2S_CHANNELS * sizeof(int16_t))

K_MEM_SLAB_DEFINE(g_audio_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

static const struct device *i2s_dev(void)
{
  return DEVICE_DT_GET(DT_ALIAS(i2s_tx));
}

/* One full sine cycle, Q15-ish amplitude, sampled by a 32-bit DDS phase
 * accumulator so any note frequency can be synthesized without floats. */
#define SINE_TABLE_LEN 256U

static const int16_t sine_table[SINE_TABLE_LEN] = {
      0,    736,   1472,   2207,   2941,   3672,   4402,   5129,
   5853,   6573,   7289,   8001,   8709,   9410,  10107,  10797,
  11481,  12157,  12827,  13488,  14142,  14787,  15423,  16050,
  16667,  17274,  17871,  18457,  19032,  19595,  20147,  20686,
  21213,  21727,  22229,  22716,  23190,  23650,  24096,  24528,
  24944,  25346,  25732,  26103,  26458,  26797,  27120,  27426,
  27716,  27990,  28246,  28486,  28708,  28913,  29101,  29271,
  29424,  29558,  29675,  29774,  29856,  29919,  29964,  29991,
  30000,  29991,  29964,  29919,  29856,  29774,  29675,  29558,
  29424,  29271,  29101,  28913,  28708,  28486,  28246,  27990,
  27716,  27426,  27120,  26797,  26458,  26103,  25732,  25346,
  24944,  24528,  24096,  23650,  23190,  22716,  22229,  21727,
  21213,  20686,  20147,  19595,  19032,  18457,  17871,  17274,
  16667,  16050,  15423,  14787,  14142,  13488,  12827,  12157,
  11481,  10797,  10107,   9410,   8709,   8001,   7289,   6573,
   5853,   5129,   4402,   3672,   2941,   2207,   1472,    736,
      0,   -736,  -1472,  -2207,  -2941,  -3672,  -4402,  -5129,
  -5853,  -6573,  -7289,  -8001,  -8709,  -9410, -10107, -10797,
 -11481, -12157, -12827, -13488, -14142, -14787, -15423, -16050,
 -16667, -17274, -17871, -18457, -19032, -19595, -20147, -20686,
 -21213, -21727, -22229, -22716, -23190, -23650, -24096, -24528,
 -24944, -25346, -25732, -26103, -26458, -26797, -27120, -27426,
 -27716, -27990, -28246, -28486, -28708, -28913, -29101, -29271,
 -29424, -29558, -29675, -29774, -29856, -29919, -29964, -29991,
 -30000, -29991, -29964, -29919, -29856, -29774, -29675, -29558,
 -29424, -29271, -29101, -28913, -28708, -28486, -28246, -27990,
 -27716, -27426, -27120, -26797, -26458, -26103, -25732, -25346,
 -24944, -24528, -24096, -23650, -23190, -22716, -22229, -21727,
 -21213, -20686, -20147, -19595, -19032, -18457, -17871, -17274,
 -16667, -16050, -15423, -14787, -14142, -13488, -12827, -12157,
 -11481, -10797, -10107,  -9410,  -8709,  -8001,  -7289,  -6573,
  -5853,  -5129,  -4402,  -3672,  -2941,  -2207,  -1472,   -736,
};

/* Equal-temperament note frequencies (Hz) used by the melody below. */
#define NOTE_C5 523U
#define NOTE_D5 587U
#define NOTE_E5 659U
#define NOTE_F5 698U
#define NOTE_G5 784U

/* Gap eaten from the end of every note's slot, for audible articulation
 * between repeated notes (e.g. the opening "E E E"). */
#define NOTE_GAP_MS 30U

/* Fade-out window at the end of each note's tone phase, in samples, so the
 * fixed-sample-count cutoff (which lands mid-cycle, not on a zero crossing)
 * doesn't produce an audible click. 5 ms at 48 kHz. */
#define NOTE_FADE_SAMPLES 240U

struct melody_note {
  uint16_t freq_hz;
  uint16_t duration_ms; /* total slot including the trailing gap */
};

/* "Jingle Bells" opening verse. */
static const struct melody_note melody[] = {
  { NOTE_E5, 300 }, { NOTE_E5, 300 }, { NOTE_E5, 600 },
  { NOTE_E5, 300 }, { NOTE_E5, 300 }, { NOTE_E5, 600 },
  { NOTE_E5, 300 }, { NOTE_G5, 300 }, { NOTE_C5, 300 }, { NOTE_D5, 300 }, { NOTE_E5, 900 },
  { NOTE_F5, 300 }, { NOTE_F5, 300 }, { NOTE_F5, 300 }, { NOTE_F5, 150 }, { NOTE_F5, 150 },
  { NOTE_E5, 300 }, { NOTE_E5, 150 }, { NOTE_E5, 150 }, { NOTE_E5, 150 },
  { NOTE_E5, 300 }, { NOTE_D5, 300 }, { NOTE_D5, 300 }, { NOTE_E5, 300 },
  { NOTE_D5, 600 }, { NOTE_G5, 900 },
};

#define MELODY_LEN ARRAY_SIZE(melody)

enum melody_phase { MELODY_PHASE_TONE, MELODY_PHASE_GAP };

static size_t g_note_index;
static enum melody_phase g_phase;
static uint32_t g_phase_samples_left;
static uint32_t g_dds_phase;
static uint32_t g_dds_step;

static void melody_enter_tone_phase(const struct melody_note *note)
{
  uint32_t tone_ms = note->duration_ms > NOTE_GAP_MS ? note->duration_ms - NOTE_GAP_MS : note->duration_ms;

  g_phase = MELODY_PHASE_TONE;
  g_phase_samples_left = (HR_I2S_SAMPLE_RATE_HZ * tone_ms) / 1000U;
  g_dds_phase = 0U;
  g_dds_step = (uint32_t)(((uint64_t)note->freq_hz << 32) / HR_I2S_SAMPLE_RATE_HZ);
}

static void melody_reset(void)
{
  g_note_index = 0U;
  melody_enter_tone_phase(&melody[0]);
}

static bool melody_active(void)
{
  return g_note_index < MELODY_LEN;
}

static int16_t melody_next_sample(void)
{
  while (g_phase_samples_left == 0U) {
    if (g_phase == MELODY_PHASE_TONE) {
      g_phase = MELODY_PHASE_GAP;
      g_phase_samples_left = (HR_I2S_SAMPLE_RATE_HZ * NOTE_GAP_MS) / 1000U;
    } else {
      g_note_index++;
      if (!melody_active()) {
        return 0;
      }
      melody_enter_tone_phase(&melody[g_note_index]);
    }
  }

  int16_t sample = 0;

  if (g_phase == MELODY_PHASE_TONE) {
    sample = sine_table[g_dds_phase >> 24];
    g_dds_phase += g_dds_step;

    /* Each note's tone phase is cut off at a fixed sample count, which lands
     * mid-cycle rather than on a zero crossing. Ramp the last few samples
     * down to silence so that cutoff doesn't produce an audible click. */
    if (g_phase_samples_left <= NOTE_FADE_SAMPLES) {
      sample = (int16_t)(((int32_t)sample * (int32_t)g_phase_samples_left) / (int32_t)NOTE_FADE_SAMPLES);
    }
  }
  g_phase_samples_left--;

  return sample;
}

static void fill_melody_block(int16_t *block)
{
  for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
    int16_t sample = melody_active() ? melody_next_sample() : 0;

    block[2 * i] = sample;
    block[2 * i + 1] = sample;
  }
}

static void play_melody(const struct device *dev)
{
  LOG_INF("playing I2S self-test melody (Jingle Bells, %u notes)", (unsigned int)MELODY_LEN);

  melody_reset();

  bool tx_started = false;
  int ret;

  while (melody_active()) {
    void *block;

    ret = k_mem_slab_alloc(&g_audio_mem_slab, &block, K_FOREVER);
    if (ret < 0) {
      LOG_ERR("melody block alloc failed: %d", ret);
      return;
    }
    fill_melody_block(block);

    ret = i2s_write(dev, block, AUDIO_BLOCK_SIZE);
    if (ret < 0) {
      LOG_ERR("i2s_write failed: %d", ret);
      return;
    }

    if (!tx_started) {
      ret = i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_START);
      if (ret < 0) {
        LOG_ERR("i2s_trigger start failed: %d", ret);
        return;
      }
      tx_started = true;
    }
  }

  ret = i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
  if (ret < 0) {
    LOG_ERR("i2s_trigger drain failed: %d", ret);
  }

  LOG_INF("I2S self-test melody finished");
}

int audio_backend_init(void)
{
  const struct device *dev = i2s_dev();

  if (!device_is_ready(dev)) {
    LOG_ERR("I2S device not ready");
    return -ENODEV;
  }

  struct i2s_config cfg = {
    .word_size = HR_I2S_WORD_SIZE_BITS,
    .channels = HR_I2S_CHANNELS,
    .format = I2S_FMT_DATA_FORMAT_I2S,
    .options = I2S_OPT_FRAME_CLK_CONTROLLER | I2S_OPT_BIT_CLK_CONTROLLER,
    .frame_clk_freq = HR_I2S_SAMPLE_RATE_HZ,
    .mem_slab = &g_audio_mem_slab,
    .block_size = AUDIO_BLOCK_SIZE,
    .timeout = 2000,
  };

  int ret = i2s_configure(dev, I2S_DIR_TX, &cfg);

  if (ret < 0) {
    LOG_ERR("I2S configure failed: %d", ret);
    return ret;
  }

  LOG_INF("audio backend init (i2s, self-test melody enabled)");
  return 0;
}

int audio_backend_start(void)
{
  LOG_INF("audio backend start (i2s)");
  play_melody(i2s_dev());
  return 0;
}

int audio_backend_stop(void)
{
  LOG_INF("audio backend stop (i2s)");
  return i2s_trigger(i2s_dev(), I2S_DIR_TX, I2S_TRIGGER_DROP);
}

#else /* !CONFIG_HR_BACKEND_I2S_TEST_TONE */

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

#endif /* CONFIG_HR_BACKEND_I2S_TEST_TONE */
