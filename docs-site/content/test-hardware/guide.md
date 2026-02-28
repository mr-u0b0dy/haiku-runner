---
title: Guide
---

## Goal

Bring up a repeatable test setup for `haiku-runner` and validate input routing behavior with minimal hardware.

## 1) Prepare workspace

From repo root:

```bash
west init -l .
west update
```

## 2) Choose test mode

### Mode A: Hardware-minimal (recommended first pass)

Use USB-C mock feeder to exercise routing without external audio hardware.

Add these options in your overlay config (or temporary config):

- `CONFIG_HR_INPUT_USB_C=y`
- `CONFIG_HR_DEFAULT_INPUT_USB_C=y`
- `CONFIG_HR_INPUT_USB_C_MOCK_FEEDER=y`

Optional tuning:

- `CONFIG_HR_INPUT_USB_C_MOCK_INTERVAL_MS=10`
- `CONFIG_HR_INPUT_USB_C_MOCK_CHUNK_BYTES=240`

### Mode B: AUX path validation

Enable AUX path and connect AUX source:

- `CONFIG_HR_INPUT_AUX=y`

### Mode C: Switching behavior checks

Enable both AUX and mock USB-C to test policy fallback and source changes.

## 3) Build and flash

```bash
west build -b nrf5340dk_nrf5340_cpuapp app --sysbuild
west flash
```

## 4) Observe runtime behavior

Check logs for:

- Input registration and activation
- Source policy decisions (default selection / fallback)
- Audio frame flow through ingress/router/pipeline path

## 5) Basic validation checklist

- Board enumerates and flashes cleanly
- Runtime logs appear consistently after reset
- Selected default source is active at startup
- Switching source does not stall frame flow
- No repeated error logs during steady-state run

## 6) Repeatable regression workflow

- Save test config snippets by scenario (`mock`, `aux`, `mixed`)
- Keep one log file per test run
- Record commit SHA with each validation note

## Troubleshooting quick hits

- Build fails: re-run `west update` and confirm Zephyr SDK/toolchain setup
- No logs: verify USB data cable and serial port selection
- No frame activity in mock mode: confirm `CONFIG_HR_INPUT_USB_C_MOCK_FEEDER=y`
