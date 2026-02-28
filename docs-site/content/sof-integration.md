---
title: SOF Integration Notes
---

SOF is not required for MVP.

When advanced DSP/pipeline control is needed:

1. Introduce SOF-backed output or pipeline adapter.
2. Keep `audio_input_ops` and `source_manager` contracts stable.
3. Add build-time switch to select Zephyr-native or SOF path.
