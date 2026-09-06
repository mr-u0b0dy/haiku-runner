---
title: Known Issues
---

Real defects, hardware limits and design shortcomings, with enough detail to judge severity rather than just a list of symptoms. Anything measured is labelled as such.

## Hardware limits (cannot be fixed in firmware)

### The nRF5340 has no classic Bluetooth radio

It is BLE-only, so **A2DP is impossible**. The only standards-based wireless audio it can receive is LE Audio. If a phone doesn't support LE Audio, there is no Bluetooth path to this device at all — the alternative is USB, AUX, or a custom BLE protocol with a companion app.

### LE Audio needs a capable source, and most devices aren't

A Bluetooth 5.2+ USB dongle, a second nRF5340 running Zephyr's `bap_unicast_client`, or a genuinely LE Audio-capable phone is required to close this out.

### 48 kHz analog capture is unreachable

The SAADC's sampling interval is expressed in whole microseconds, so achievable rates are 1 MHz divided by an integer; 48 kHz would need 20.833 µs. Cross-referenced against the I2S rates derivable from the 12.288 MHz audio clock (48000/32000/24000/16000/12000/8000), **8 kHz is the only rate exactly representable on both sides**.

Consequence: at the 47619 Hz default, capture and playback clocks differ by ~0.8%, producing a slow buffer drift and a pitch error of roughly 14 cents.

### Analog capture is mono only

Zephyr's nRF SAADC driver arms the SAADC hardware sampling timer only when exactly one channel is active (`active_channel_cnt == 1` in `adc_nrfx_saadc.c`). With two channels it paces samples from a kernel timer, which cannot reach audio intervals — `adc_read()` fails with `-EBUSY`. A `BUILD_ASSERT` catches this at compile time. Harmless in practice, since the MAX98357A output stage is mono anyway.

## Firmware shortcomings

### USB audio has no clock-drift regulation

`feedback_cb` returns a fixed nominal value (Q10.14, `48 << 14`) rather than regulating against the audio clock. The host's frame clock and the board's I2S clock drift apart, so the buffer creeps until a block is dropped or the transmitter underruns. A stalled transmitter is detected and reset, but that is damage control, not a fix.

Not observed as audible in a 70-second test, but it will surface over longer playback. The correct fix is a feedback regulator — Zephyr's `uac2_explicit_feedback` sample implements one.

### AUX capture pauses between frames

The ADC is re-armed per frame. Double buffering means the gap is only thread wake-up latency rather than the whole convert-and-deliver cycle, but it is not zero, so the long-run capture rate sits slightly below playback.

Measured underruns over 25 s, showing each fix's contribution:

| Configuration | Underruns / 25 s |
| --- | --- |
| 47619 Hz, 10 ms frames, single-buffered | 43 + 74 stalls |
| 8 kHz, 10 ms frames, single-buffered | 17 |
| 8 kHz, 10 ms frames, ping-pong | 6 |
| 8 kHz, 40 ms frames, ping-pong | 2 |
| 47619 Hz, 10 ms frames, ping-pong (**default**) | 22 |

The proper fix is continuous double-buffered SAADC DMA via `nrfx` directly — the hardware supports seamless buffer chaining, but Zephyr's per-sequence ADC API does not expose it.

### An unwired AUX input hijacks arbitration

With `CONFIG_HR_INPUT_AUX=y` and nothing connected, the floating ADC pin reads enough noise to clear the signal threshold. AUX then declares itself healthy, wins arbitration whenever the preferred source goes idle, and produces roughly one underrun per second.

**Do not enable AUX until the analog front-end is built.** Raising `CONFIG_HR_INPUT_AUX_SIGNAL_THRESHOLD` mitigates it, but the real answer is a biased, filtered, low-impedance front-end.

### Volume control is reported but not applied

VCS is registered and volume changes from the phone are acknowledged and logged, but nothing scales the PCM. The MAX98357A has fixed analog gain, so applying volume means digital scaling in the pipeline.

### Switching sources causes an audible discontinuity

BLE and USB run at 48 kHz, AUX at 47619 Hz, so changing to or from AUX makes the backend reconfigure the I2S peripheral — a momentary drop. Acceptable at a deliberate source change; it is the steady-state mismatch above that matters more.

### Bonds are stored, but there is no way to clear them

Bonds persist to NVS and survive reboot. There is no UI or command to forget a peer, so a stale bond can only be cleared by reflashing or erasing the storage partition.

### Only one connection is supported

`CONFIG_BT_MAX_CONN=1`. Appropriate for a speaker, but it means an app holding a GATT connection (a debugging tool, say) blocks the phone's audio stack from connecting at all. This caused real confusion during development.

## Testing and process gaps

### There is no automated test suite

No unit tests, no integration tests, no CI beyond a compile check. Every functional claim in these docs rests on manual observation of a serial log. Regressions in pipeline logic would not be caught.

The pipeline is unusually testable — `audio_input_ops`, `audio_backend`, and `input_frame_ingress` are all narrow, mockable interfaces — so this is a missed opportunity rather than a hard problem.

### The serial console is the only observability

There is no shell, no runtime introspection, no counters exposed. Diagnosing anything means reading logs — and the J-Link VCOM drops bytes under load, so log messages are frequently truncated or lost, which made several bugs harder to pin down than they should have been.
