---
title: Components
---

## Required

| Item | Qty | Purpose |
| --- | ---: | --- |
| nRF5340 DK (`nrf5340dk_nrf5340_cpuapp`) | 1 | Main target board |
| USB data cable (board-compatible) | 1 | Flash, power, and serial logs |
| Host PC with Zephyr toolchain + `west` | 1 | Build and test control |

## Recommended

| Item | Qty | Purpose |
| --- | ---: | --- |
| Powered USB hub | 1 | Stable power and cable management |
| Breadboard jumpers | Set | Fast wiring for GPIO experiments |
| Label tape or cable tags | 1 | Prevent connector mistakes |

## Optional (by test focus)

| Item | Qty | Purpose |
| --- | ---: | --- |
| 3.5 mm AUX source device | 1 | Validate AUX adapter path |
| Headphones / powered speaker | 1 | Audible output checks |
| USB audio source device | 1 | Future non-mock USB path validation |
| Logic analyzer or oscilloscope | 1 | Timing/signal-level debugging |
| MAX98357A I2S amp module | 1 | Real I2S audio output path (replaces stub backend) — see [MAX98357A Wiring](/test-hardware/max98357a-wiring) |

## Software config components

Use these Kconfig toggles based on scenario:

- `CONFIG_HR_INPUT_AUX=y`
- `CONFIG_HR_INPUT_USB=y`
- `CONFIG_HR_INPUT_USB_MOCK_FEEDER=y`
- `CONFIG_HR_DEFAULT_INPUT_USB=y`

See [Guide](/test-hardware/guide) for a complete workflow.

## AUX analog front-end

The 3.5 mm line input needs a small bias/anti-alias circuit before the SAADC pin — a 1 µF coupling capacitor, a 10 kΩ/10 kΩ divider to VDD/2, and an RC low-pass. Full circuit, values and rationale in [AUX Jack Wiring](/test-hardware/aux-jack-wiring).
