# Test Hardware

This directory contains a practical test bench definition for validating `haiku-runner` during bring-up and regression checks.

## Contents

- [Layout](layout.md): Physical bench wiring/layout for quick and repeatable tests
- [Components](components.md): Required hardware, optional hardware, and recommended tools
- [Guide](guide.md): Step-by-step setup and validation procedure

## Scope

The test bench is focused on:

- `nrf5340dk_nrf5340_cpuapp`
- Audio input path validation (AUX / USB-C mock / Wi-Fi placeholder)
- Audio routing and source selection behavior
- Hardware-assisted and hardware-minimal workflows
