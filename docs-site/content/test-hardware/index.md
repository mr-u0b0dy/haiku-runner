---
title: Test Hardware
---

This section contains a practical test bench definition for validating `haiku-runner` during bring-up and regression checks.

## Contents

- [Layout](/test-hardware/layout): Physical bench wiring/layout for quick and repeatable tests
- [Components](/test-hardware/components): Required hardware, optional hardware, and recommended tools
- [Guide](/test-hardware/guide): Step-by-step setup and validation procedure
- [MAX98357A Wiring](/test-hardware/max98357a-wiring): Board-to-module pin connections for I2S audio output
- [AUX Jack Wiring](/test-hardware/aux-jack-wiring): Analog front-end required for the 3.5 mm line input

## Scope

The test bench is focused on:

- `nrf5340dk/nrf5340/cpuapp`
- Audio input path validation (AUX / USB mock / Wi-Fi stub)
- Audio routing and source selection behavior
- Hardware-assisted and hardware-minimal workflows
