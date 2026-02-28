---
title: Input Model
---

Each input implements:

- `init`
- `start`
- `stop`
- `healthy`
- `set_frame_callback`

Registration is explicit at boot through `input_adapter_*_register()` calls.

## Hybrid switching

- Manual selection sets preferred source.
- Auto fallback can move to another healthy source if current one fails.
- Source health is currently adapter-defined and intentionally simple.
