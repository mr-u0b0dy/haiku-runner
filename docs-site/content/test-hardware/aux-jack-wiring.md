---
title: AUX Jack Wiring
---

The AUX input samples a line-level analog signal with the nRF5340's SAADC. Unlike the I2S and USB paths, **this one does not work with a bare wire from a headphone jack** — it needs a small external front-end. This page explains why, gives the circuit, and lists the limitations you should know before building it.

## Why a front-end is required

Audio from a phone or laptop headphone output is **AC-coupled and bipolar**: it swings above and below 0 V, typically ±1.4 V peak for a loud source.

The SAADC here is configured single-ended with a 0–VDD (0–3.3 V) input span. Feed it a raw audio signal and the entire negative half of the waveform is clamped at 0 — you get gross distortion, and you risk pulling an SoC pin below ground, which is outside its absolute-maximum rating.

The front-end therefore does three jobs:

1. **Block DC** from the source with a series capacitor.
2. **Bias the signal to VDD/2** (1.65 V) so it sits mid-scale and can swing both directions.
3. **Low-pass filter** it below Nyquist so out-of-band content doesn't alias down into the audible band.

## Circuit

One channel (the firmware captures mono — see [Limitations](#limitations)):

```
 jack tip ──┐
            │
           ┌┴┐ 1 µF          R3 1 kΩ
 (audio) ──┤ ├──────┬────────/\/\/\──────┬──── P0.04 (AIN0 / A0)
           └┬┘      │                    │
            │       │                   ─┴─ C2
 jack ──────┴───┐   │                   ─┬─ 8.2 nF
 sleeve         │   │                    │
                │   ├── R1 10 kΩ ── VDD  │
               GND  │                    │
                    ├── R2 10 kΩ ── GND  │
                    │                    │
                   ─┴─ C1 10 µF          │
                   ─┬─ (bias decoupling) │
                    │                    │
                   GND                  GND
```

| Part | Value | Purpose |
| --- | --- | --- |
| C_in | 1 µF film or ceramic | DC block from the source |
| R1 / R2 | 10 kΩ / 10 kΩ | VDD/2 bias divider |
| C1 | 10 µF | Stiffens the bias node so audio doesn't modulate it |
| R3 | 1 kΩ | Series element of the anti-alias filter |
| C2 | 8.2 nF | Anti-alias to ground (~19 kHz corner) |

### Why these values

**The divider must be 10 kΩ, not 100 kΩ.** The SAADC channel uses a 3 µs acquisition time, which the nRF5340 only guarantees for a source impedance up to about 10 kΩ. A 10 kΩ/10 kΩ divider presents ~5 kΩ Thevenin, and R3 brings the total to ~6 kΩ — inside spec. A 100 kΩ/100 kΩ divider (a common suggestion for generic bias networks) presents 50 kΩ, and the sample-and-hold will not settle: you get attenuated, smeared readings. If you must use higher resistors, raise `zephyr,acquisition-time` in the overlay to 10 µs and re-check that two acquisitions still fit inside the sampling interval.

**The filter corner follows your sample rate.** With the default 47619 Hz sampling, Nyquist is ~23.8 kHz, and 8.2 nF puts the corner at ~19 kHz. If you run the drift-free 8 kHz mode (see below), Nyquist is only 4 kHz — change C2 to **47 nF** (~3.4 kHz corner), or everything above 4 kHz folds back into the audio as aliasing.

**Headroom.** Bias at 1.65 V leaves ±1.65 V of swing, which comfortably fits a ±1.4 V peak source. If your source is hotter and you hear clipping, add a divider (e.g. 10 kΩ series with 10 kΩ to ground) ahead of C_in.

## Pin connections

| Signal | nRF5340 DK | nRF53 pin | Note |
| --- | --- | --- | --- |
| Front-end output | `A0` | P0.04 (AIN0) | Analog header |
| Bias supply | `3V3` | — | Feeds R1 |
| Ground | `GND` | — | Also the jack sleeve |

`A1` / P0.05 (AIN1) is already declared in the overlay as a second channel for future stereo use, but the firmware does not sample it — see below.

## Enabling it

```bash
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild --pristine -- \
  -DCONFIG_HR_BACKEND_I2S=y -DCONFIG_HR_INPUT_AUX=y -DCONFIG_HR_DEFAULT_INPUT_AUX=y
west flash
```

On boot you should see:

```
<inf> input_aux_jack: AUX adapter registered
<inf> input_aux_jack: AUX capture ready (47619 Hz, 1 ch, 12-bit ADC)
<inf> input_aux_jack: AUX capture started
```

and, once audio is actually present:

```
<inf> input_aux_jack: AUX signal detected (peak ...)
<inf> audio_backend_i2s: I2S configured: 47619 Hz, 1920 bytes/block
```

The adapter reports itself **unhealthy while the input is silent**, so `source_manager`'s auto-fallback doesn't sit on a disconnected jack. Tune with `CONFIG_HR_INPUT_AUX_SIGNAL_THRESHOLD` and `CONFIG_HR_INPUT_AUX_SILENCE_FRAMES`. If front-end noise keeps it permanently "healthy", raise the threshold.

## Limitations

These are real and measured, not theoretical. Read them before deciding this input is what you want.

**Mono only.** Zephyr's nRF SAADC driver only arms the SAADC's hardware sampling timer when exactly one channel is active; with two it paces samples from a kernel timer, which cannot run at audio intervals and makes `adc_read()` fail outright. Since the MAX98357A output stage is mono anyway, the firmware samples the left channel only. To hear both channels, passively sum them into the front-end with a 10 kΩ resistor from each of tip and ring.

**The capture and playback clocks do not match.** The SAADC's sampling interval is expressed in whole microseconds, so the achievable rates are 1 MHz divided by an integer — 48 kHz (20.833 µs) is not among them. The I2S output clock, derived from the 12.288 MHz audio clock, can only produce 48000/32000/24000/16000/12000/8000 Hz. **8000 Hz (interval 125) is the only rate exactly representable on both sides.** At the 47619 Hz default the two clocks differ by ~0.8%, so the output slowly starves and the driver reports an underrun roughly once a second.

**Capture pauses briefly between frames.** The ADC is re-armed per frame, and the gap between one capture completing and the next being queued is time during which no samples are taken. The firmware uses two buffers ping-pong so this gap is only the thread wake-up latency rather than the whole conversion and delivery, but it is not zero, and it puts the long-run capture rate slightly below playback.

Measured underruns over 25 seconds on a real board:

| Configuration | Underruns / 25 s |
| --- | --- |
| 47619 Hz, 10 ms frames, single-buffered | 43 (plus 74 stalls) |
| 8000 Hz, 10 ms frames, single-buffered | 17 |
| 8000 Hz, 10 ms frames, ping-pong | 6 |
| **8000 Hz, 40 ms frames, ping-pong** | **2** |
| 47619 Hz, 10 ms frames, ping-pong (default) | 22 |

For the cleanest currently-available path, at the cost of 4 kHz audio bandwidth:

```
CONFIG_HR_INPUT_AUX_SAMPLE_INTERVAL_US=125
CONFIG_HR_INPUT_AUX_FRAME_SAMPLES=320
```

Properly fixing this needs either a sample-rate converter between capture and playback, or continuous double-buffered SAADC DMA via `nrfx` directly (the SAADC hardware supports seamless buffer chaining; Zephyr's per-sequence ADC API does not expose it).

**Quality ceiling.** This is a 12-bit ADC reading a passively-biased signal, then re-emitting it through a DAC. It will never match the USB or BLE paths, which carry 16-bit samples end to end. Analog in → digitise → analog out only earns its keep if you want DSP in the middle; for pure passthrough, a direct cable to the amplifier is better in every respect.

## Safety notes

- Keep the AIN pin between 0 V and VDD at all times. If you're experimenting with unknown sources, add Schottky clamp diodes from the pin to VDD and GND.
- Connect the jack sleeve to board ground. If the source device is also connected to the board over USB, you now have two ground paths — hum from that loop is common; a ground-loop isolator on the audio input fixes it.
- Build it on a breadboard with short leads. Long unshielded runs into a high-impedance analog node pick up everything, including the board's own switching noise.
