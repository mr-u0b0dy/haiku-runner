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
- **`GAIN`:** a static 5-level strap pin, not a logic input — the amp samples it once and latches a gain of 3/6/9/12/15 dB depending on whether it reads `GND`, `VDD` (i.e. `VIN`), floating, or one of two `100 kΩ` dividers to `GND`/`VDD`. **All five recognized states are referenced to `VIN`, not to an independent rail.** Tying `GAIN` to a supply other than `VIN` itself (e.g. the DK's `3V3` while `VIN` is on `5V`) lands the pin at some fraction of `VIN` that isn't one of the five defined points — an out-of-spec, undefined gain state, not a clean design choice. Leave it fully floating (9 dB, the default assumed elsewhere in this doc) or tie it directly to `GND` or to the same `VIN` net — never to a different rail.

## Confirmed case: GAIN pin tied to a different rail than VIN

One build had `VIN` correctly on `5V` but `GAIN` wired to the DK's separate `3V3` pin — i.e. two different supplies, not the same net twice. At `VIN` = 5 V, `3V3` is 66% of `VDD`, which doesn't correspond to any of the five documented `GAIN` states (`GND`=0%, `100k`-to-`GND`, floating ≈ mid-rail, `100k`-to-`VDD`, `VDD`=100%) — it sits between "floating" and "tied to `VDD`", outside the pin's designed operating points. That's a plausible explanation for audible quality problems specifically at high output: an undefined strap state has no datasheet guarantee of behaving like any single gain setting, consistently or otherwise.

**Fix:** re-wire `GAIN` to land on one of the five defined states — floating (simplest, matches the rest of this doc), tied to `GND`, or tied to the *same* `VIN` net (not a separate supply pin at a different voltage even if it happens to be close). If you want a specific dB tap via the resistor-divider states, use the actual `VIN` net as the divider's reference, per your breakout's datasheet.

## VIN and headroom at high volume

`VIN` sets how much clean output swing the amp has before it runs out of rail to swing to — this is a hard ceiling regardless of how loud the digital signal claims to be. **Measured with a logic analyzer on `OUT+`/`OUT-`** (channels probed directly at the amp's speaker terminals, 12.5 MS/s analog capture during live playback at max device volume): the switching waveform topped out around **3.26 V**. On a build where `VIN` is confirmed on `5V`, a ceiling that low points at the `GAIN` mis-wiring above rather than supply headroom — worth re-measuring after fixing `GAIN` to see whether the ceiling moves. If `VIN` genuinely is on `3V3` on your build, that alone caps headroom well below what the amp can do at 5 V and is worth fixing regardless.

The recovered audio-band signal (raw PWM low-pass filtered back down to baseband) didn't show hard flat-top clipping in that same capture — crest factor ~4.9, no pileup of samples at the peak — so on that particular clip the amp wasn't yet slamming into its ceiling. That doesn't rule out clipping on louder material; it means the margin is thinner than it should be, not that it's already gone. `volume_control` (`app/src/audio/volume_control.c`) now soft-limits digital peaks above 80% of full scale for exactly this reason — see [Known Issues](/known-issues).

## Verifying the connection with a self-test melody

Once wired per the table above, you can play a short synthesized "Jingle Bells" melody directly out of I2S0 — independent of any input adapter or pipeline frame — to confirm the amp and wiring work before trusting the rest of the audio path. A melody with varying pitch is a better bring-up check than a single flat tone: it exercises the DDS/I2S timing across several frequencies instead of just one, and it's easy to tell "recognizable tune" from "garbled noise" by ear.

Enable it with `configs/i2s-test-tone.conf` (`CONFIG_HR_BACKEND_I2S=y` plus
`CONFIG_HR_BACKEND_I2S_TEST_TONE=y`):

```bash
west build -b nrf5340dk/nrf5340/cpuapp app --sysbuild -- -Dapp_EXTRA_CONF_FILE=configs/i2s-test-tone.conf
west flash
```

On boot you should see `audio backend init (i2s, self-test tone enabled)` followed by `playing I2S self-test melody (Jingle Bells, 26 notes)` in the log, and hear the opening verse of "Jingle Bells" (~9 seconds) from the speaker attached to the MAX98357A. If you hear nothing:

- Re-check `BCLK`/`LRC`/`DIN`/`GND`/`VIN` against the pin table above — a swapped `LRC`/`DIN` is the most common mistake.
- Confirm `SD` isn't accidentally pulled low (shutdown).
- Confirm the amp's `VIN` is actually powered (measure it) — the DK's `5V` pin is only live when the board is powered via the appropriate USB/power connector, not always through debug-only connections.

If you hear it *sometimes* rather than never, suspect a bad connection over a firmware bug — one confirmed case: logic-analyzer probes on the two speaker leads (`OUT+`/`OUT-`) showed a strong tone-correlated signal on one leg and a flat noise floor on the other, in both the idle and playing windows. A single open/marginal speaker-wire connection (loose breadboard contact, bad solder joint) produces exactly this "buzzes sometimes" symptom, since the amp only drives audible sound when both legs happen to make contact. Check/reseat both `OUT+` and `OUT-` at the amp and at the speaker before suspecting the I2S signals themselves — a self-test-melody run with clean, continuous `BCLK`/`LRC`/`DIN` (no dropouts across the whole ~9s window) rules out the SoC/I2S side and points the problem downstream, at the amp's output or the speaker wiring.

This only exercises the backend in isolation at boot; it does not touch `source_manager` or any input adapter. `audio_backend_write()` itself is not test-tone-specific — with `CONFIG_HR_BACKEND_I2S=y` (regardless of `CONFIG_HR_BACKEND_I2S_TEST_TONE`) it forwards real pipeline PCM from whichever input is active to `i2s0`; see [Architecture](/architecture) for how a frame gets there.

## Notes

- MAX98357A is **mono**. With `DIN` fed a normal stereo I2S stream and `SD` left floating, it outputs a `(L+R)/2` downmix. For true stereo you need two modules (and typically a way to select L-only / R-only per channel, which some breakouts expose via extra pins).
- `app/boards/nrf5340dk_nrf5340_cpuapp.overlay` already carries the `i2s0` pinctrl/clock config matching the table above (adapted from Zephyr's `i2s/output` sample), so no extra devicetree work is needed to run the self-test tone. With `CONFIG_HR_BACKEND_I2S=y`, `audio_backend_i2s.c` drives real hardware either way: the self-test melody at boot if `CONFIG_HR_BACKEND_I2S_TEST_TONE=y`, and real pipeline PCM from the active input regardless. Only with `CONFIG_HR_BACKEND_I2S=n` (the default) does the stub backend take over and I2S is never touched.
