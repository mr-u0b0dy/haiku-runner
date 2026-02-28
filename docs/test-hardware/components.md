# Test Components

## Required

| Item | Qty | Purpose |
|---|---:|---|
| nRF5340 DK (`nrf5340dk_nrf5340_cpuapp`) | 1 | Main target board |
| USB data cable (board-compatible) | 1 | Flash, power, and serial logs |
| Host PC with Zephyr toolchain + `west` | 1 | Build and test control |

## Recommended

| Item | Qty | Purpose |
|---|---:|---|
| Powered USB hub | 1 | Stable power and cable management |
| Breadboard jumpers | Set | Fast wiring for GPIO experiments |
| Label tape or cable tags | 1 | Prevent connector mistakes |

## Optional (by test focus)

| Item | Qty | Purpose |
|---|---:|---|
| 3.5 mm AUX source device | 1 | Validate AUX adapter path |
| Headphones / powered speaker | 1 | Audible output checks |
| USB audio source device | 1 | Future non-mock USB-C path validation |
| Logic analyzer or oscilloscope | 1 | Timing/signal-level debugging |

## Software config components

Use these Kconfig toggles based on scenario:

- `CONFIG_HR_INPUT_AUX=y`
- `CONFIG_HR_INPUT_USB_C=y`
- `CONFIG_HR_INPUT_USB_C_MOCK_FEEDER=y`
- `CONFIG_HR_DEFAULT_INPUT_USB_C=y`

See [Guide](guide.md) for a complete workflow.
