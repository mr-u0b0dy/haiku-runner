---
title: Build
---

## Prerequisites

- Zephyr SDK installed
- `west` installed
- Toolchain dependencies for your OS

## Initialize

```bash
west init -l .
west update
```

## Build for nRF53 DK

```bash
west build -b nrf5340dk_nrf5340_cpuapp app --sysbuild
```

## Common config toggles

- Enable AUX adapter: `CONFIG_HR_INPUT_AUX=y`
- Enable USB-C adapter: `CONFIG_HR_INPUT_USB_C=y`
- Enable Wi-Fi adapter: `CONFIG_HR_INPUT_WIFI=y`

## USB-C mock feeder (no hardware)

To validate end-to-end routing without a USB source, add these to an overlay config:

- `CONFIG_HR_INPUT_USB_C=y`
- `CONFIG_HR_DEFAULT_INPUT_USB_C=y`
- `CONFIG_HR_INPUT_USB_C_MOCK_FEEDER=y`

Optional tuning:

- `CONFIG_HR_INPUT_USB_C_MOCK_INTERVAL_MS=10`
- `CONFIG_HR_INPUT_USB_C_MOCK_CHUNK_BYTES=240`

For full bench setup, component list, and validation flow, see [Test Hardware](/test-hardware).
