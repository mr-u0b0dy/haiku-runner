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

## USB audio input

With `CONFIG_HR_INPUT_USB_C=y` the board enumerates as a USB Audio Class 2 sound card
("Haiku Runner Speaker"), so any host — a PC, or a phone in USB OTG mode — can stream to it
with no drivers, no app and no pairing.

Use the connector labelled **nRF USB**, not the other one: on the nRF5340 DK both are
**micro-USB**, and the second is the onboard J-Link (debug/serial/power). Despite the
`HR_INPUT_USB_C` Kconfig name, nothing here requires a USB-C connector — the name refers to a
future custom board, and the protocol is connector-agnostic.

```bash
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild --pristine -- \
  -DCONFIG_HR_BACKEND_I2S=y -DCONFIG_HR_INPUT_USB_C=y -DCONFIG_HR_DEFAULT_INPUT_USB_C=y
```

Known limitation: the UAC2 feedback endpoint currently reports a fixed nominal rate rather than
regulating to the audio clock. The host's frame clock and the board's I2S clock therefore drift
slowly apart; a stalled I2S transmitter is detected and reset, but a proper feedback regulator
(see Zephyr's `uac2_explicit_feedback` sample) is the real fix.

## USB-C mock feeder (no hardware)

To validate end-to-end routing without a USB source, add these to an overlay config:

- `CONFIG_HR_INPUT_USB_C=y`
- `CONFIG_HR_DEFAULT_INPUT_USB_C=y`
- `CONFIG_HR_INPUT_USB_C_MOCK_FEEDER=y`

Optional tuning:

- `CONFIG_HR_INPUT_USB_C_MOCK_INTERVAL_MS=10`
- `CONFIG_HR_INPUT_USB_C_MOCK_CHUNK_BYTES=240`

For full bench setup, component list, and validation flow, see [Test Hardware](/test-hardware).
