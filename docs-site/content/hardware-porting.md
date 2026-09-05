---
title: Hardware Porting
---

For new output hardware:

1. Keep source manager and input APIs unchanged.
2. Implement backend in `app/src/audio/`.
3. Add Kconfig flag under `Kconfig.audio`.
4. Add board overlay / pinmux updates in `app/boards/`.

For new input hardware:

1. Add adapter in `app/src/input/`.
2. Implement `audio_input_ops`, delivering audio via `input_frame_ingress_deliver()`.
3. Give `healthy()` a real meaning — a constant `true` breaks auto-fallback for every other source.
4. Add an id to `enum audio_input_id`, an LED entry in `source_control.c` (a `BUILD_ASSERT` enforces one per id) and a cue pattern in `audio_cue.c`.
5. Add the `CONFIG_HR_INPUT_*` flag and wire the source file into `app/CMakeLists.txt` behind it.
6. Register adapter in `main.c`.

See [Input Model](/input-model) for the full contract.

## Porting to a different board

Beyond the usual overlay work, three things in this repo are nRF5340 DK specific:

- **Net-core Bluetooth image.** `app/sysbuild.cmake` builds Zephyr's `hci_ipc` for the nRF5340 network core. A single-core SoC with an on-chip controller needs this removed.
- **Audio clock.** The overlay sets `hfclkaudio` to 12.288 MHz, which is the 48 kHz family. The achievable I2S rates follow from it, and the AUX sample rate is constrained against it.
- **Front-panel control.** `source_control.c` requires `sw0` and `led0`..`led3` aliases.
