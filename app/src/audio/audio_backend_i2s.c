#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "audio_backend.h"

LOG_MODULE_REGISTER(audio_backend_i2s, LOG_LEVEL_INF);

#define HR_I2S_SAMPLE_RATE_HZ 48000U
#define HR_I2S_CHANNELS       2U
#define HR_I2S_WORD_SIZE_BITS 16U

/* Slab blocks are sized for the worst case a BLE Audio (LC3) frame can
 * produce: 10 ms at 48 kHz, stereo, 16-bit. The rate and frame duration are
 * negotiated per stream, so the I2S peripheral is configured lazily to match
 * whatever actually arrives (see i2s_apply_config), and the block size used
 * per write can be smaller than the slab block. */
#define AUDIO_BLOCK_SAMPLES 480U
#define AUDIO_BLOCK_COUNT   6U
#define AUDIO_BLOCK_SIZE (AUDIO_BLOCK_SAMPLES * HR_I2S_CHANNELS * sizeof(int16_t))

K_MEM_SLAB_DEFINE(g_audio_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

static const struct device *i2s_dev(void)
{
  return DEVICE_DT_GET(DT_ALIAS(i2s_tx));
}

/* Tracks whether TX is currently running, so the first writes after
 * start/stop kick the I2S peripheral off. */
static bool g_tx_started;

/* Blocks queued since the last start/stop. Starting TX on the very first
 * block leaves no cushion: the DMA begins draining immediately and any
 * jitter in the source starves it ("Next buffers not supplied on time").
 * Prime a few blocks first so there is slack to absorb it. */
#define I2S_PRIME_BLOCKS 3U
static uint32_t g_queued_blocks;

/* Consecutive failed block allocations before we treat TX as stalled and
 * reset it. A couple of failures are normal transient backpressure. */
#define I2S_STALL_THRESHOLD 4U
static uint32_t g_alloc_failures;

/* Format the peripheral is currently configured for; 0 means unconfigured. */
static uint32_t g_cfg_rate_hz;
static size_t g_cfg_block_bytes;

static int i2s_apply_config(uint32_t rate_hz, size_t block_bytes)
{
  const struct device *dev = i2s_dev();

  struct i2s_config cfg = {
    .word_size = HR_I2S_WORD_SIZE_BITS,
    .channels = HR_I2S_CHANNELS,
    .format = I2S_FMT_DATA_FORMAT_I2S,
    .options = I2S_OPT_FRAME_CLK_CONTROLLER | I2S_OPT_BIT_CLK_CONTROLLER,
    .frame_clk_freq = rate_hz,
    .mem_slab = &g_audio_mem_slab,
    .block_size = block_bytes,
    .timeout = 2000,
  };

  int ret = i2s_configure(dev, I2S_DIR_TX, &cfg);

  if (ret < 0) {
    LOG_ERR("I2S configure failed (%u Hz, %zu bytes): %d", rate_hz, block_bytes, ret);
    g_cfg_rate_hz = 0U;
    return ret;
  }

  g_cfg_rate_hz = rate_hz;
  g_cfg_block_bytes = block_bytes;
  LOG_INF("I2S configured: %u Hz, %zu bytes/block", rate_hz, block_bytes);
  return 0;
}

int audio_backend_write(const struct audio_frame *frame)
{
  if (frame == NULL || frame->data == NULL || frame->size == 0U) {
    return -EINVAL;
  }

  if (frame->bits_per_sample != 16U || frame->sample_rate_hz == 0U) {
    LOG_WRN("unsupported frame format (%u-bit, %u Hz)", frame->bits_per_sample,
           frame->sample_rate_hz);
    return -ENOTSUP;
  }

  if (frame->channels != 1U && frame->channels != 2U) {
    LOG_WRN("unsupported channel count: %u", frame->channels);
    return -ENOTSUP;
  }

  const size_t in_samples = frame->size / (frame->channels * sizeof(int16_t));
  const size_t block_bytes = in_samples * HR_I2S_CHANNELS * sizeof(int16_t);

  if (in_samples == 0U || block_bytes > AUDIO_BLOCK_SIZE) {
    LOG_WRN("frame of %zu samples exceeds the %u-sample I2S block", in_samples,
           AUDIO_BLOCK_SAMPLES);
    return -ENOTSUP;
  }

  const struct device *dev = i2s_dev();
  int ret;

  /* Re-configure on the fly if the stream's rate or frame size changed
   * (a new ASE can negotiate anything from 8 kHz/7.5 ms upwards). */
  if (frame->sample_rate_hz != g_cfg_rate_hz || block_bytes != g_cfg_block_bytes) {
    if (g_tx_started) {
      (void)i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
      g_tx_started = false;
    }
    g_queued_blocks = 0U;

    ret = i2s_apply_config(frame->sample_rate_hz, block_bytes);
    if (ret < 0) {
      return ret;
    }
  }

  void *block;

  ret = k_mem_slab_alloc(&g_audio_mem_slab, &block, K_NO_WAIT);
  if (ret < 0) {
    /* The driver hands blocks back to the slab as it transmits them, so a
     * slab that stays empty means TX is no longer draining: either the
     * source paused and the DMA underran into the driver's error state, or
     * it was never started. Neither recovers on its own, and every later
     * write would fail the same way, so reset the peripheral and let the
     * next frames re-prime it. */
    if (++g_alloc_failures >= I2S_STALL_THRESHOLD) {
      LOG_WRN("I2S TX stalled, resetting");
      if (i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_PREPARE) < 0) {
        (void)i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
      }
      g_tx_started = false;
      g_queued_blocks = 0U;
      g_alloc_failures = 0U;
    }
    return ret;
  }

  g_alloc_failures = 0U;

  const int16_t *src = (const int16_t *)(const void *)frame->data;
  int16_t *dst = (int16_t *)block;

  for (size_t i = 0; i < in_samples; ++i) {
    if (frame->channels == 1U) {
      dst[2 * i] = src[i];
      dst[2 * i + 1] = src[i];
    } else {
      dst[2 * i] = src[2 * i];
      dst[2 * i + 1] = src[2 * i + 1];
    }
  }

  ret = i2s_write(dev, block, block_bytes);
  if (ret < 0) {
    /* The driver only takes ownership of the block on success, so this one is
     * ours to release - returning without freeing leaks it out of the slab,
     * and a handful of failures permanently starves every later write. */
    k_mem_slab_free(&g_audio_mem_slab, block);

    /* A write typically fails because an underrun put the peripheral in
     * I2S_STATE_ERROR, and PREPARE is the only transition out of it. It is
     * rejected in any other state, which is harmless here. */
    if (i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_PREPARE) < 0) {
      (void)i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
    }

    g_tx_started = false;
    g_queued_blocks = 0U;

    LOG_WRN("i2s_write failed (%d), TX reset", ret);
    return ret;
  }

  g_queued_blocks++;

  if (!g_tx_started && g_queued_blocks >= I2S_PRIME_BLOCKS) {
    ret = i2s_trigger(dev, I2S_DIR_TX, I2S_TRIGGER_START);
    if (ret < 0) {
      LOG_ERR("i2s_trigger start failed: %d", ret);
      return ret;
    }
    g_tx_started = true;
  }

  return 0;
}

#if defined(CONFIG_HR_BACKEND_I2S_TEST_TONE)

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

  /* The melody is synthesized at a fixed 48 kHz using full-size blocks; the
   * peripheral is otherwise configured lazily by audio_backend_write(). */
  if (i2s_apply_config(HR_I2S_SAMPLE_RATE_HZ, AUDIO_BLOCK_SIZE) < 0) {
    return;
  }

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

#endif /* CONFIG_HR_BACKEND_I2S_TEST_TONE */

int audio_backend_init(void)
{
  const struct device *dev = i2s_dev();

  if (!device_is_ready(dev)) {
    LOG_ERR("I2S device not ready");
    return -ENODEV;
  }

  /* Deliberately no i2s_configure() here: the sample rate and block size
   * depend on what the active stream negotiates, so configuration happens on
   * the first write (or when play_melody runs). */
  LOG_INF("audio backend init (i2s)");
  return 0;
}

int audio_backend_start(void)
{
  LOG_INF("audio backend start (i2s)");

#if defined(CONFIG_HR_BACKEND_I2S_TEST_TONE)
  play_melody(i2s_dev());
#endif

  return 0;
}

int audio_backend_stop(void)
{
  LOG_INF("audio backend stop (i2s)");

  g_queued_blocks = 0U;

  if (!g_tx_started) {
    /* Nothing queued, and the peripheral may never have been configured. */
    return 0;
  }

  g_tx_started = false;
  return i2s_trigger(i2s_dev(), I2S_DIR_TX, I2S_TRIGGER_DROP);
}
