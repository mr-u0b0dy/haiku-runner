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
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild
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

## Ready-made build variants

`app/configs/` has a Kconfig fragment for each hardware combination this project actually
exercises, so you don't have to assemble `-DCONFIG_...` flags by hand. `.github/workflows/build.yml`
builds all seven (the six fragments plus the plain default) on every push, so a break in any one of
them fails CI instead of hiding behind the default's stub backend.

| Fragment | Backend | Inputs |
| --- | --- | --- |
| *(none — the plain default)* | stub | BLE only |
| `configs/i2s-ble.conf` | I2S | BLE only |
| `configs/i2s-usb.conf` | I2S | USB only |
| `configs/i2s-aux.conf` | I2S | AUX only |
| `configs/aux-usb-ble.conf` | stub | AUX + USB + BLE (arbitration/fallback exercise) |
| `configs/usb-mock-feeder.conf` | stub | USB, synthetic feeder instead of a real device |
| `configs/i2s-test-tone.conf` | I2S | BLE, plus the self-test melody on boot |

Build with one via `app_EXTRA_CONF_FILE`, e.g.:

```bash
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild -- -Dapp_EXTRA_CONF_FILE=configs/i2s-ble.conf
```

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

(`configs/i2s-usb.conf` does the same, plus disabling BLE to isolate the USB path — see
[Ready-made build variants](#ready-made-build-variants).)

Known limitation: the UAC2 feedback endpoint currently reports a fixed nominal rate rather than
regulating to the audio clock. The host's frame clock and the board's I2S clock therefore drift
slowly apart; a stalled I2S transmitter is detected and reset, but a proper feedback regulator
(see Zephyr's `uac2_explicit_feedback` sample) is the real fix.

## USB mock feeder (no hardware)

To validate end-to-end routing without a USB source, use `configs/usb-mock-feeder.conf` (or add
these to your own overlay config):

- `CONFIG_HR_INPUT_USB=y`
- `CONFIG_HR_DEFAULT_INPUT_USB=y`
- `CONFIG_HR_INPUT_USB_MOCK_FEEDER=y`

Optional tuning:

- `CONFIG_HR_INPUT_USB_MOCK_INTERVAL_MS=10`
- `CONFIG_HR_INPUT_USB_MOCK_CHUNK_BYTES=240`

For full bench setup, component list, and validation flow, see [Test Hardware](/test-hardware).
