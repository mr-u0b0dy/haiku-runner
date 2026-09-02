---
title: MAX98357A Wiring
---

The MAX98357A is a mono I2S Class-D amplifier — no MCLK required, just bit clock, word clock, and serial data. This is the reference wiring for driving one from the `nrf5340dk_nrf5340_cpuapp` I2S0 peripheral.

## Pin connection table

| nRF5340 DK (Arduino header) | nRF53 pin | MAX98357A pin | Signal |
| --- | --- | --- | --- |
| `D13` | P1.15 | `BCLK` | I2S bit clock |
| `D10` | P1.12 | `LRC` | I2S word select (L/R clock) |
| `D11` | P1.13 | `DIN` | I2S serial data |
| `5V` (or `3V3`) | — | `VIN` | Amp supply (2.5–5.5 V; use `5V` for full power) |
| `GND` | — | `GND` | Common ground |

These are the same `P1.12` / `P1.13` / `P1.15` assignments Zephyr's upstream `samples/drivers/i2s/output` sample uses for this exact board, so they're known-good with the SoC's I2S0 pin options.

## SD and GAIN pins

- **`SD` (shutdown):** most MAX98357A breakouts have an onboard pull-up, so leaving it unconnected enables the amp at boot. Wire it to a spare GPIO instead if you want software-controlled mute/enable.
- **`GAIN`:** leave floating for the default 9 dB gain. If you need a different gain tap, check your specific breakout's datasheet — the float/GND/VIN/resistor-divider options vary slightly by revision.

