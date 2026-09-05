# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

`haiku-runner` is a Zephyr RTOS application targeting the `nrf5340dk/nrf5340/cpuapp` board: a BLE speaker scaffold with a multi-input audio architecture (BLE, AUX jack, USB audio, Wi-Fi streaming). It is early-stage: the BLE input (LE Audio unicast sink), the USB input (USB Audio Class 2 device) and the I2S output are real and verified on hardware, but the AUX and Wi-Fi adapters are still placeholders or stubs, not full hardware drivers (and Wi-Fi is not possible on this board at all — the nRF5340 has no Wi-Fi radio). The repo is a `west` workspace root (self-managed manifest in `west.yml`, pulling in upstream Zephyr `v4.4.2`). The LE Audio (BAP/CAP/VCP/TMAP) APIs this app uses require v4.4.x — they do not exist in v4.0.0.

There are two independent parts of this repo:
- `app/` — the Zephyr firmware application (C).
- `docs-site/` — a `shadcn-docs-nuxt` documentation site (Nuxt/Vue/TypeScript), unrelated tooling from the firmware.

## Commands

### Firmware (from repo root)

```bash
# One-time workspace init (pulls Zephyr per west.yml)
west init -l .
west update

# Build for the target board
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild
```

There is no unit test suite for the firmware in this repo. Validation is done by building and, for input adapters lacking hardware, via config-enabled mock feeders (e.g. `CONFIG_HR_INPUT_USB_MOCK_FEEDER=y`) — see `docs-site/content/build.md`.

Useful Kconfig toggles (set via `prj.conf`, an overlay `.conf`, or `west build -- -DCONFIG_...=y`):
- `CONFIG_HR_INPUT_AUX=y`, `CONFIG_HR_INPUT_USB=y`, `CONFIG_HR_INPUT_WIFI=y` — enable input adapters (BLE is on by default).
- `CONFIG_HR_DEFAULT_INPUT_{BLE,AUX,USB,WIFI}` — choice of default active source at boot.
- `CONFIG_HR_INPUT_USB_MOCK_FEEDER=y` — synthetic PCM feeder thread for USB path testing without hardware (see `Kconfig.inputs` for interval/chunk/stack tuning).
- `CONFIG_HR_INPUT_USB_UAC2=y` — enumerate as a real USB Audio Class 2 sound card (default when `HR_INPUT_USB` is on); disable to use the mock feeder instead.
- `CONFIG_HR_BACKEND_I2S=y` — drive real audio out over I2S instead of the stub backend. Note the default is the *stub*, so a plain build produces a silent device.
- `CONFIG_HR_PIPELINE_SOF=y` — compile in the SOF pipeline adapter seam (not required for MVP).

CI (`.github/workflows/build.yml`) does exactly: `west init -l .` → `west update` → install Zephyr's `scripts/requirements.txt` → install `gcc-arm-none-eabi` → `west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild`. Mirror this sequence when validating build changes locally.

### Docs site (from `docs-site/`)

```bash
npm install
npm run dev      # local dev server, http://localhost:3000
```

Content lives in `docs-site/content/*.md` (Nuxt Content). This is what should be updated when architecture, build steps, or the input model change.

## Architecture

The firmware is built around a source-agnostic audio pipeline with a strict separation between input adapters, source arbitration, and output backend — designed so BLE is not a privileged/central component:

```
input adapter(s) --> source_manager (arbitration) --> audio_router --> audio_backend (output)
```

1. **Input adapters** (`app/src/input/*.c`, `app/src/ble/le_audio_sink.c`) each implement the `struct audio_input_ops` contract from `app/include/audio_input.h`: `init`, `start`, `stop`, `poll`, `healthy`, `set_frame_callback`. Adapters are registered explicitly at boot in `app/src/main.c::register_enabled_inputs()`, gated by their `CONFIG_HR_INPUT_*` Kconfig flags, via `input_adapter_*_register()` functions declared in `app/include/input_adapters.h`. The BLE adapter (`le_audio_sink.c`) is a thin wrapper around `app/src/ble/le_audio_bap_sink.c`, which owns the actual Bluetooth LE Audio (BAP) unicast sink stack — advertising, PACS/ASCS, ISO stream, and LC3 decode — and calls back into `le_audio_sink_receive_frame()` / `le_audio_on_stream_ready()` / `le_audio_on_stream_lost()`.
2. **`input_registry`** (`input_registry.c`/`.h`) holds the set of registered `audio_input_descriptor`s (id + name + ops).
3. **`source_manager`** (`source_manager.c`/`.h`) owns which input is active, per a `struct source_policy` (`source_policy.h`): `SOURCE_MODE_MANUAL` or `SOURCE_MODE_HYBRID`, with `allow_auto_fallback` letting it switch away from an unhealthy source automatically. It's driven by periodic `source_manager_tick()` calls from the main loop and receives frames via `source_manager_on_frame()`.
4. **`audio_router`** (`audio_router.c`/`.h`) forwards accepted frames (`struct audio_frame`, see `audio_frame.h`) from the active source to the backend.
5. **`audio_backend`** (`audio_backend.h`) is the hardware-agnostic output API (`init`/`start`/`stop`/`write`). Only one backend implementation is compiled in, chosen in `app/CMakeLists.txt` by `CONFIG_HR_BACKEND_I2S` (else falls back to `audio_backend_stub.c`).
6. **`input_frame_ingress`** (`input_frame_ingress.c`/`.h`) is a shared helper adapters use to validate/deliver a raw PCM chunk into an `audio_input_frame_callback_t`, checking `source_ready` and building `audio_frame` metadata (sample rate, channels, bits per sample) consistently.
7. **SOF integration** is an optional seam, not a hard dependency: `CONFIG_HR_PIPELINE_SOF` conditionally compiles `audio_pipeline_sof_adapter.c` (`app/include/audio_pipeline.h` is the pipeline-level API it would plug into) without touching `audio_input_ops` or `source_manager` contracts.

### Porting guidance (from `docs-site/content/hardware-porting.md`)

- New output hardware: implement a new backend in `app/src/audio/`, add its Kconfig flag under `Kconfig.audio`, add board overlay/pinmux in `app/boards/` — leave `source_manager`/input APIs unchanged.
- New input hardware: add an adapter in `app/src/input/`, implement `audio_input_ops`, register it in `main.c`.

### Kconfig structure

`app/Kconfig` sources `Kconfig.inputs` (per-input enable flags + USB tuning + hybrid-switch fallback flag) and `Kconfig.audio` (backend selection, SOF pipeline toggle, default-input choice) under the `Haiku Runner` menu. When adding a new input or backend, extend the relevant `Kconfig.*` file rather than `app/Kconfig` directly, and wire the new source file into `app/CMakeLists.txt` behind its `CONFIG_HR_*` guard.

### Board specifics

- Board config: `app/boards/nrf5340dk_nrf5340_cpuapp.conf` / `.overlay` (currently minimal — console on `uart0`).
- `app/Kconfig.sysbuild` + `app/sysbuild.cmake` build the nRF5340 network-core Bluetooth controller image (Zephyr's `samples/bluetooth/hci_ipc`, peripheral-only ISO config) alongside the app-core image whenever `CONFIG_HR_INPUT_BLE` is enabled — required for the BLE Audio (LE Audio unicast sink) input to have a controller to talk to. `west build --sysbuild` produces both images.
- `app/sysbuild.conf` is intentionally minimal, reserved for future nRF53 multi-image controls.

## Documentation

Architecture/build/porting docs already exist under `docs-site/content/` (`architecture.md`, `build.md`, `input-model.md`, `sof-integration.md`, `hardware-porting.md`, `test-hardware/`). When making architectural changes, prefer updating those alongside code rather than duplicating explanations elsewhere.
