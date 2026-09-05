---
title: Roadmap
---

Pending work, ordered by what unblocks the most. Each item says why it matters and what "done" looks like, so it can be picked up cold.

## Immediate — finish what is already built

### Verify the source button, LEDs and audible cue

Built and initialising, never actually pressed. Confirm: one press cycles to the next registered source, the correct LED lights, the beep pattern matches (1 = BLE, 2 = USB, 3 = AUX), a press during playback doesn't wedge the pipeline, and holding or double-pressing behaves sanely.

### Prove LE Audio actually streams

The sink is spec-complete and a phone bonds and discovers all five services, but **no audio has ever flowed**. Cheapest route is a Bluetooth 5.2+ USB dongle for the development PC, which makes the BlueZ/PipeWire configuration already worked out usable. Alternatives: a second nRF5340 running Zephyr's `bap_unicast_client`, or a genuinely LE Audio-capable phone.

Until this is done, the largest subsystem in the project is unproven.

### Build the AUX analog front-end

The [wiring guide](/test-hardware/aux-jack-wiring) has the circuit. Until it exists, AUX cannot be judged, and enabling it without the circuit actively degrades behaviour.

### Make CI build the real configurations

CI compiles only the default (stub backend, no optional inputs), so none of the I2S, USB, or AUX code is covered. Move the seven-variant matrix into the workflow.

## Near term — correctness

### USB clock-drift feedback regulator

Replace the fixed nominal `feedback_cb` with a real regulator so the host tracks the board's audio clock. Zephyr's `uac2_explicit_feedback` sample is the reference. Removes the slow buffer creep that currently ends in a dropped block.

### Continuous SAADC capture

Move AUX capture to `nrfx` with chained buffers so sampling never pauses between frames. Eliminates the residual underrun trickle that per-sequence re-arming causes.

### Sample-rate conversion

A resampler between capture and playback would let AUX run at full bandwidth without the 0.8% clock mismatch, and would decouple every input's native rate from the output clock. This is the single change that most improves audio correctness across all three inputs.

### Apply VCP volume to the PCM stream

Volume changes are acknowledged and ignored. Digital scaling in the pipeline would make the phone's volume control real.

### Automated tests

`audio_input_ops`, `audio_backend` and `input_frame_ingress` are narrow, mockable interfaces. Unit tests for frame ingress validation, source arbitration and fallback would catch pipeline regressions that manual log-reading will not.

## Longer term — capability

### Bond management

A way to forget peers — long-press on the button, or a shell command. Currently a stale bond can only be cleared by reflashing.

### Runtime shell

A Zephyr shell with commands for source selection, pipeline counters and bond management would replace log-scraping as the diagnostic mechanism, and is far cheaper than the debugging time it saves.

### Custom BLE audio transport

For phones without LE Audio, LC3 or Opus frames over an L2CAP connection-oriented channel would work with **any** BLE phone. liblc3 is already linked in, so the firmware side is largely written; the cost is a companion app, since this cannot integrate with the OS media output.

### Wi-Fi input

Requires an nRF7002 companion — the nRF5340 has no Wi-Fi radio. Would unlock AirPlay/DLNA/RTP. The Wi-Fi adapter stub anticipates this.

### Stereo output

Everything downstream of the pipeline is mono because the MAX98357A is a mono amplifier. Stereo needs a second amplifier or a stereo DAC, plus honest stereo handling in the backend rather than the current duplicate-to-both-slots.

### Power management

No sleep states, no idle handling, no battery awareness. A speaker that stays fully awake is not a shippable product.

## Explicitly out of scope

- **Classic Bluetooth / A2DP** — the nRF5340 radio cannot do it.
- **On-board Wi-Fi** — no radio; needs a companion chip.
- **SOF pipeline** — the seam exists (`CONFIG_HR_PIPELINE_SOF`) but there is no implementation and no near-term need.
