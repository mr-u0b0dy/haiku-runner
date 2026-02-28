# Architecture

`haiku-runner` is organized around a source-agnostic audio pipeline:

1. Input adapters produce `audio_frame` packets.
2. `source_manager` arbitrates active source using hybrid policy.
3. `audio_router` forwards frames to selected backend.
4. Backend implementation drives physical output (stub now, I2S later).

## Multi-input model

- BLE is treated as one adapter, not the central pipeline owner.
- Future adapters (AUX, USB-C, Wi-Fi) implement the same `audio_input_ops` API.
- Switching policy defaults to manual selection with auto fallback on unhealthy source.

## SOF integration seam

SOF is optional and can be integrated by adding a new backend or pipeline adapter without changing input adapter contracts.
