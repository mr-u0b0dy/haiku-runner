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
2. Implement `audio_input_ops`.
3. Register adapter in `main.c`.
