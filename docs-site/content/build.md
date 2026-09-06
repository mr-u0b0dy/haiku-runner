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

## Debugging

`.vscode/launch.json` has `cortex-debug` launch configs for the nRF5340 app core (J-Link and
OpenOCD variants), both pointing at `build/app/zephyr/zephyr.elf` — the sysbuild default domain
for this repo — and at the vendored SVD `app/boards/nrf5340dk_nrf5340_cpuapp.svd` (Nordic's
official nRF5340 application-core SVD) for peripheral register info. Build first, then either:

- **VS Code**: install the [cortex-debug](https://github.com/Marus/cortex-debug) extension, open
  the repo folder, and use the Run and Debug panel.
- **Neovim**: install [nvim-dap](https://github.com/mfussenegger/nvim-dap) and
  [nvim-dap-cortex-debug](https://github.com/jedrzejboczar/nvim-dap-cortex-debug), which reads the
  same `launch.json` automatically. cortex-debug's backend itself still needs installing once
  (e.g. `:MasonInstall cortex-debug` if using mason.nvim).

The net-core `hci_ipc` image isn't covered by these configs — it's an unmodified upstream Zephyr
sample, not app code.

## Common config toggles

- Enable AUX adapter: `CONFIG_HR_INPUT_AUX=y`
- Enable USB audio adapter: `CONFIG_HR_INPUT_USB=y`
- Enable Wi-Fi adapter: `CONFIG_HR_INPUT_WIFI=y`
- Disable the BLE Audio input (and its Bluetooth stack + net-core image): `CONFIG_HR_INPUT_BLE=n`
- Enable the I2S output backend: `CONFIG_HR_BACKEND_I2S=y`
- Play a self-test melody on I2S start, independent of any input: `CONFIG_HR_BACKEND_I2S_TEST_TONE=y`

## USB audio input

With `CONFIG_HR_INPUT_USB=y` the board enumerates as a USB Audio Class 2 sound card
("Haiku Runner Speaker"), so any host — a PC, or a phone in USB OTG mode — can stream to it
with no drivers, no app and no pairing.

Use the connector labelled **nRF USB**, not the other one: on the nRF5340 DK both are
**micro-USB**, and the second is the onboard J-Link (debug/serial/power). USB Audio Class is
connector-agnostic — micro-B, USB-C or USB-A all carry the same protocol.

```bash
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild --pristine -- \
  -DCONFIG_HR_BACKEND_I2S=y -DCONFIG_HR_INPUT_USB=y -DCONFIG_HR_DEFAULT_INPUT_USB=y
```

Known limitation: the UAC2 feedback endpoint currently reports a fixed nominal rate rather than
regulating to the audio clock. The host's frame clock and the board's I2S clock therefore drift
slowly apart; a stalled I2S transmitter is detected and reset, but a proper feedback regulator
(see Zephyr's `uac2_explicit_feedback` sample) is the real fix.

## USB mock feeder (no hardware)

To validate end-to-end routing without a USB source, add these to an overlay config:

- `CONFIG_HR_INPUT_USB=y`
- `CONFIG_HR_DEFAULT_INPUT_USB=y`
- `CONFIG_HR_INPUT_USB_MOCK_FEEDER=y`

Optional tuning:

- `CONFIG_HR_INPUT_USB_MOCK_INTERVAL_MS=10`
- `CONFIG_HR_INPUT_USB_MOCK_CHUNK_BYTES=240`

For full bench setup, component list, and validation flow, see [Test Hardware](/test-hardware).
