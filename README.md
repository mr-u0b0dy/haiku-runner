# haiku-runner

Zephyr-based BLE speaker scaffold for `nrf5340dk_nrf5340_cpuapp` with a multi-input architecture.

## Current scope

- Upstream Zephyr application layout
- LE Audio sink path represented as an input adapter boundary (skeleton)
- Source manager with hybrid switching policy (manual selection + auto fallback)
- Hardware-agnostic output backend API
- Placeholder input adapters for AUX jack, USB Type-C audio, and Wi-Fi audio
- Optional SOF adapter seam (not required in MVP)

## Repository layout

- `app/` Zephyr app
- `docs/` architecture and bring-up guides
- `docs/test-hardware/` test bench layout, components, and setup guide
- `.github/workflows/` CI build workflow

## Quick start

1. Install Zephyr SDK + `west` prerequisites.
2. Initialize workspace from this repo root:
   - `west init -l .`
   - `west update`
3. Build:
   - `west build -b nrf5340dk_nrf5340_cpuapp app --sysbuild`

See `docs/build.md` for details.
See `docs/test-hardware/README.md` for test bench documentation.

## License

Licensed under the Apache License, Version 2.0. See `LICENSE`.
