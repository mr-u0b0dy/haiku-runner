/* AUX jack input: samples a line-level analog signal with the SAADC and
 * feeds it into the audio pipeline through the shared ingress path.
 *
 * The SAADC is single-ended here and its input range is 0..VDD, so the
 * external front-end biases the (bipolar, AC-coupled) audio signal to VDD/2.
 * Mid-scale therefore corresponds to silence, and this file converts the
 * unsigned ADC counts back into signed PCM.
 */
#include <errno.h>
#include <stdlib.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "input_aux_jack.h"
#include "input_frame_ingress.h"
#include "input_registry.h"

LOG_MODULE_REGISTER(input_aux_jack, LOG_LEVEL_INF);

#define AUX_CHANNELS      CONFIG_HR_INPUT_AUX_CHANNELS
#define AUX_FRAME_SAMPLES CONFIG_HR_INPUT_AUX_FRAME_SAMPLES
#define AUX_RESOLUTION    CONFIG_HR_INPUT_AUX_ADC_RESOLUTION
#define AUX_INTERVAL_US   CONFIG_HR_INPUT_AUX_SAMPLE_INTERVAL_US

/* The sampling interval is expressed in whole microseconds, so the rate is
 * whatever 1 MHz divides to - not necessarily a standard audio rate. It is
 * reported to the pipeline as-is rather than rounded to 48 kHz, so the I2S
 * backend configures the closest clock it can rather than silently playing
 * the stream at the wrong speed. See Kconfig.inputs for the caveat. */
#define AUX_SAMPLE_RATE_HZ (1000000U / AUX_INTERVAL_US)

/* Mid-scale for a single-ended reading at the configured resolution: the
 * bias voltage the front-end applies, i.e. digital silence. */
#define AUX_MID_SCALE (1 << (AUX_RESOLUTION - 1))

/* Left-shift that scales an AUX_RESOLUTION-bit sample up to 16-bit PCM. */
#define AUX_PCM_SHIFT (16 - AUX_RESOLUTION)

static const struct adc_dt_spec g_aux_channels[] = {
  ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
#if AUX_CHANNELS > 1
  ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
#endif
};

BUILD_ASSERT(ARRAY_SIZE(g_aux_channels) == AUX_CHANNELS,
             "AUX channel count must match the io-channels in the overlay");

/* Zephyr's nRF SAADC driver only arms the SAADC's own sampling timer when a
 * single channel is active; with more it paces samples from a kernel timer,
 * which cannot reach audio intervals and makes adc_read() fail. */
BUILD_ASSERT(AUX_CHANNELS == 1 || AUX_INTERVAL_US > 128,
             "Stereo AUX capture cannot use the SAADC hardware timer - "
             "set CONFIG_HR_INPUT_AUX_CHANNELS=1");

/* One sampling of the sequence yields one sample per enabled channel, so a
 * frame is FRAME_SAMPLES samplings interleaved across the channels.
 *
 * Two buffers, used ping-pong: the next capture is queued before the previous
 * frame is converted and pushed downstream. Sampling otherwise stops for the
 * duration of that work, which puts the long-run capture rate below the I2S
 * playback rate - a deficit no amount of output buffering can absorb, so it
 * surfaces as a steady trickle of underruns. */
static int16_t g_frames[2][AUX_FRAME_SAMPLES * AUX_CHANNELS];

static audio_input_frame_callback_t g_callback;
static bool g_plugged;
static bool g_running;
static uint32_t g_silent_frames;

static K_THREAD_STACK_DEFINE(g_aux_stack, CONFIG_HR_INPUT_AUX_STACK_SIZE);
static struct k_thread g_aux_thread;
static struct k_sem g_aux_run;
static struct k_poll_signal g_async_signal;

static int aux_configure_channels(void)
{
  for (size_t i = 0; i < ARRAY_SIZE(g_aux_channels); ++i) {
    if (!adc_is_ready_dt(&g_aux_channels[i])) {
      LOG_ERR("ADC device not ready for channel %zu", i);
      return -ENODEV;
    }

    int ret = adc_channel_setup_dt(&g_aux_channels[i]);

    if (ret != 0) {
      LOG_ERR("adc_channel_setup_dt(%zu) failed: %d", i, ret);
      return ret;
    }
  }

  return 0;
}

/* Converts the raw sequence buffer in place from single-ended ADC counts to
 * signed 16-bit PCM, and reports the frame's peak amplitude so the caller can
 * tell signal from silence. */
static uint16_t aux_convert_frame(int16_t *frame, size_t count)
{
  int32_t peak = 0;

  for (size_t i = 0; i < count; ++i) {
    int32_t sample = (int32_t)frame[i] - AUX_MID_SCALE;

    sample <<= AUX_PCM_SHIFT;

    if (sample > INT16_MAX) {
      sample = INT16_MAX;
    } else if (sample < INT16_MIN) {
      sample = INT16_MIN;
    }

    frame[i] = (int16_t)sample;

    int32_t magnitude = sample < 0 ? -sample : sample;

    if (magnitude > peak) {
      peak = magnitude;
    }
  }

  return (uint16_t)peak;
}

/* An unplugged jack still reads a steady bias voltage, which would look like
 * a perfectly healthy source to source_manager and stop auto-fallback from
 * ever moving away. Treat a sustained silent input as unhealthy instead. */
static void aux_update_presence(uint16_t peak)
{
  if (peak >= CONFIG_HR_INPUT_AUX_SIGNAL_THRESHOLD) {
    if (!g_plugged) {
      LOG_INF("AUX signal detected (peak %u)", peak);
    }
    g_silent_frames = 0U;
    g_plugged = true;
    return;
  }

  if (!g_plugged) {
    return;
  }

  if (++g_silent_frames >= CONFIG_HR_INPUT_AUX_SILENCE_FRAMES) {
    LOG_INF("AUX silent, reporting unhealthy");
    g_plugged = false;
    g_silent_frames = 0U;
  }
}

static void aux_sample_thread(void *p1, void *p2, void *p3)
{
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  struct adc_sequence_options options = {
    .interval_us = AUX_INTERVAL_US,
    .extra_samplings = AUX_FRAME_SAMPLES - 1,
    .callback = NULL,
  };
  struct adc_sequence sequence = {
    .options = &options,
    .buffer_size = sizeof(g_frames[0]),
    .resolution = AUX_RESOLUTION,
  };

  for (size_t i = 0; i < ARRAY_SIZE(g_aux_channels); ++i) {
    sequence.channels |= BIT(g_aux_channels[i].channel_id);
  }

  struct k_poll_event event =
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &g_async_signal, 0);
  uint8_t index = 0U;
  bool capturing = false;

  while (true) {
    /* Blocks until the adapter is started, and again if it is stopped. */
    k_sem_take(&g_aux_run, K_FOREVER);
    k_sem_give(&g_aux_run);

    if (!capturing) {
      sequence.buffer = g_frames[index];
      k_poll_signal_reset(&g_async_signal);

      int ret = adc_read_async(g_aux_channels[0].dev, &sequence, &g_async_signal);

      if (ret != 0) {
        LOG_ERR("adc_read_async failed: %d", ret);
        k_sleep(K_MSEC(100));
        continue;
      }
      capturing = true;
    }

    int ret = k_poll(&event, 1, K_MSEC(1000));

    if (ret != 0) {
      LOG_WRN("AUX capture timed out: %d", ret);
      capturing = false;
      continue;
    }

    event.state = K_POLL_STATE_NOT_READY;
    k_poll_signal_reset(&g_async_signal);

    /* Queue the next capture before touching the finished one, so the ADC
     * keeps sampling while this frame is converted and delivered. */
    const uint8_t done = index;

    index ^= 1U;
    sequence.buffer = g_frames[index];

    ret = adc_read_async(g_aux_channels[0].dev, &sequence, &g_async_signal);
    if (ret != 0) {
      LOG_ERR("adc_read_async failed: %d", ret);
      capturing = false;
    }

    uint16_t peak = aux_convert_frame(g_frames[done], ARRAY_SIZE(g_frames[done]));

    aux_update_presence(peak);

    (void)input_aux_receive_frame((const uint8_t *)g_frames[done], sizeof(g_frames[done]),
                                  AUX_SAMPLE_RATE_HZ, AUX_CHANNELS, 16U);
  }
}

static int aux_poll(void)
{
  return 0;
}

static int aux_init(void)
{
  g_plugged = false;
  g_silent_frames = 0U;

  int ret = aux_configure_channels();

  if (ret != 0) {
    return ret;
  }

  k_sem_init(&g_aux_run, 0, 1);
  k_poll_signal_init(&g_async_signal);

  k_thread_create(&g_aux_thread, g_aux_stack, K_THREAD_STACK_SIZEOF(g_aux_stack),
                  aux_sample_thread, NULL, NULL, NULL,
                  CONFIG_HR_INPUT_AUX_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&g_aux_thread, "aux_sample");

  LOG_INF("AUX capture ready (%u Hz, %u ch, %u-bit ADC)", AUX_SAMPLE_RATE_HZ,
          AUX_CHANNELS, AUX_RESOLUTION);
  return 0;
}

static int aux_start(void)
{
  if (!g_running) {
    g_running = true;
    k_sem_give(&g_aux_run);
    LOG_INF("AUX capture started");
  }

  return 0;
}

static int aux_stop(void)
{
  if (g_running) {
    g_running = false;
    (void)k_sem_take(&g_aux_run, K_NO_WAIT);
  }

  g_plugged = false;
  return 0;
}

static bool aux_healthy(void)
{
  return g_plugged;
}

static int aux_set_callback(audio_input_frame_callback_t callback)
{
  g_callback = callback;
  return 0;
}

int input_aux_set_plugged(bool plugged)
{
  g_plugged = plugged;
  return 0;
}

int input_aux_receive_frame(const uint8_t *data,
                            size_t size,
                            uint32_t sample_rate_hz,
                            uint8_t channels,
                            uint8_t bits_per_sample)
{
  return input_frame_ingress_deliver(g_callback,
                                     g_plugged,
                                     AUDIO_INPUT_AUX,
                                     data,
                                     size,
                                     sample_rate_hz,
                                     channels,
                                     bits_per_sample);
}

static const struct audio_input_ops g_aux_ops = {
  .init = aux_init,
  .start = aux_start,
  .stop = aux_stop,
  .poll = aux_poll,
  .healthy = aux_healthy,
  .set_frame_callback = aux_set_callback,
};

static const struct audio_input_descriptor g_aux_descriptor = {
  .id = AUDIO_INPUT_AUX,
  .name = "aux",
  .ops = &g_aux_ops,
};

int input_adapter_aux_register(void)
{
  LOG_INF("AUX adapter registered");
  return input_registry_register(&g_aux_descriptor);
}
