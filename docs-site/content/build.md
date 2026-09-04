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

`--sysbuild` is required, not optional: the BLE input (on by default) is a Bluetooth LE Audio
unicast sink, which needs a second image built for the nRF5340 network core (the BLE controller,
`samples/bluetooth/hci_ipc` from upstream Zephyr) alongside the application-core image. This is
wired up by `app/Kconfig.sysbuild` and `app/sysbuild.cmake`. Flash both images produced by the
build (`west flash` after a sysbuild picks these up automatically).

## Common config toggles

- Enable AUX adapter: `CONFIG_HR_INPUT_AUX=y`
- Enable USB-C adapter: `CONFIG_HR_INPUT_USB_C=y`
- Enable Wi-Fi adapter: `CONFIG_HR_INPUT_WIFI=y`
- Disable the BLE Audio input (and its Bluetooth stack + net-core image): `CONFIG_HR_INPUT_BLE=n`
- Enable the I2S output backend: `CONFIG_HR_BACKEND_I2S=y`
- Play a self-test melody on I2S start, independent of any input: `CONFIG_HR_BACKEND_I2S_TEST_TONE=y`

## USB-C mock feeder (no hardware)

To validate end-to-end routing without a USB source, add these to an overlay config:

- `CONFIG_HR_INPUT_USB_C=y`
- `CONFIG_HR_DEFAULT_INPUT_USB_C=y`
- `CONFIG_HR_INPUT_USB_C_MOCK_FEEDER=y`

Optional tuning:

- `CONFIG_HR_INPUT_USB_C_MOCK_INTERVAL_MS=10`
- `CONFIG_HR_INPUT_USB_C_MOCK_CHUNK_BYTES=240`

For full bench setup, component list, and validation flow, see [Test Hardware](/test-hardware).
