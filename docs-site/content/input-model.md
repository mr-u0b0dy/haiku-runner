---
title: Input Model
---

Every input looks the same to the rest of the system. That is the point: the pipeline, arbitration and output layers contain no per-source special cases.

## The adapter contract

Each adapter implements `struct audio_input_ops` from `app/include/audio_input.h`:

| Op | Purpose |
| --- | --- |
| `init` | One-time setup — bring up the stack or peripheral this input needs |
| `start` | Begin accepting audio (advertise, enumerate, start sampling) |
| `stop` | Stop accepting audio |
| `poll` | Called every `source_manager_tick()`; for periodic work or retries |
| `healthy` | Is this source currently able to supply audio? |
| `set_frame_callback` | Where to deliver frames |

Adapters register explicitly at boot via `input_adapter_*_register()`, called from `main.c::register_enabled_inputs()` behind their `CONFIG_HR_INPUT_*` flags. There is no automatic discovery — what a build contains is visible in one function.

## Delivering audio

Adapters do not talk to the router or the backend. They call `input_frame_ingress_deliver()`, which checks the source is ready and builds the `audio_frame` metadata consistently. Because format travels with each frame, inputs running at different rates can coexist in one build.

## What "healthy" means

Health is how arbitration decides, so each adapter defines it meaningfully rather than returning a constant:

- **BLE** — a LE Audio stream is started (`le_audio_on_stream_ready()` / `_lost()`).
- **USB** — the host has enabled the audio streaming terminal.
- **AUX** — the input is above the silence threshold. Deliberately *not* "the ADC is running": an unplugged jack still reads a steady bias voltage and would otherwise look permanently healthy, pinning arbitration to a dead input.

## Arbitration

`source_manager_tick()` runs from the main loop every 200 ms:

1. Poll every registered adapter.
2. If the **preferred** source (`CONFIG_HR_DEFAULT_INPUT_*`, or whatever was last selected) is healthy, make it active.
3. Otherwise, if the current source is still healthy, stay.
4. Otherwise, fall back to the first healthy source.

Steps 2–4 apply only in `SOURCE_MODE_HYBRID` with `CONFIG_HR_SWITCH_ALLOW_AUTO_FALLBACK=y`. Frames from any non-active source are dropped in `source_manager_on_frame()`.

The practical effect: plug in USB and it takes over; unplug it and the device falls back to whatever else has signal.

## Choosing a source manually

`source_manager_select()` sets the preferred source. It is driven by the board button through `source_control`, which cycles only the adapters actually registered in the build, lights one LED per source, and announces the change through the speaker.

Without that control, switching is purely automatic — for a long time `source_manager_select()` existed with no caller at all.

## Adding an input

1. Add the adapter under `app/src/input/`, implementing `audio_input_ops`.
2. Deliver audio via `input_frame_ingress_deliver()` — do not call the router directly.
3. Give `healthy()` a real meaning; a constant `true` breaks fallback for every other source.
4. Add an id to `enum audio_input_id`, an LED entry in `source_control.c` (a `BUILD_ASSERT` enforces one per id), and a cue pattern in `audio_cue.c`.
5. Add the `CONFIG_HR_INPUT_*` flag in `Kconfig.inputs` and wire the source file into `app/CMakeLists.txt` behind it.
6. Register it in `main.c::register_enabled_inputs()`.
