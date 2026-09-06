---
title: Components
---

## Required

| Item | Qty | Purpose |
| --- | ---: | --- |
| nRF5340 DK (`nrf5340dk/nrf5340/cpuapp`) | 1 | Main target board |
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
| USB audio source device (PC, or phone in OTG mode) | 1 | Real (non-mock) USB path validation — verified end to end, see [Project Status](/status) |
| Logic analyzer or oscilloscope | 1 | Timing/signal-level debugging |
| MAX98357A I2S amp module | 1 | Real I2S audio output path (replaces stub backend) — see [MAX98357A Wiring](/test-hardware/max98357a-wiring) |

## Software config components

`app/configs/` has a ready-made Kconfig fragment per scenario (`i2s-aux.conf`,
`usb-mock-feeder.conf`, `aux-usb-ble.conf`, ...) — see [Build](/build#ready-made-build-variants)
for the full list — rather than assembling `CONFIG_HR_INPUT_*` toggles by hand.

See [Guide](/test-hardware/guide) for a complete workflow.

## AUX analog front-end

The 3.5 mm line input needs a small bias/anti-alias circuit before the SAADC pin — a 1 µF coupling capacitor, a 10 kΩ/10 kΩ divider to VDD/2, and an RC low-pass. Full circuit, values and rationale in [AUX Jack Wiring](/test-hardware/aux-jack-wiring).
