---
title: Architecture
---

`haiku-runner` is organized around a source-agnostic audio pipeline:

1. Input adapters produce `audio_frame` packets.
2. `source_manager` arbitrates active source using hybrid policy.
3. `audio_router` forwards frames to selected backend.
4. Backend implementation drives physical output (stub, or real I2S when `CONFIG_HR_BACKEND_I2S=y`).

## Multi-input model

- BLE is treated as one adapter, not the central pipeline owner.
- Other adapters (USB audio, and the AUX / Wi-Fi stubs) implement the same `audio_input_ops` API.
- Switching policy defaults to manual selection with auto fallback on unhealthy source.

## BLE Audio input

The BLE input adapter is a Bluetooth LE Audio (BAP) unicast sink, not a passive stub: a phone or
other LE Audio source connects directly to the device and streams LC3-encoded audio, the same way
it would pair with a standard BLE speaker (classic A2DP isn't an option — the nRF5340 radio is
BLE-only). It advertises a single sink Audio Stream Endpoint, decodes the incoming LC3 stream to
PCM in the BT RX thread, and delivers frames through the normal
`le_audio_sink_receive_frame()` → `input_frame_ingress` → `source_manager` path like any other
adapter. Stream start/stop is reported via `le_audio_on_stream_ready()`/`le_audio_on_stream_lost()`,
which drives the adapter's `healthy()` state (and therefore hybrid auto-fallback).

Building this requires a second firmware image for the nRF5340 network core (the Bluetooth
controller) — see `app/Kconfig.sysbuild` / `app/sysbuild.cmake` and the [Build](/build) page.

Note that LE Audio needs a capable *source*: the phone or PC must support LE Audio unicast
(Bluetooth 5.2+ with OS support). Many Android devices and most PC Bluetooth adapters do not,
in which case the device pairs and exposes its services correctly but no audio stream is ever
established.

## USB audio input

The USB adapter implements a real USB Audio Class 2 playback device, so a host sees the board as
a plain USB sound card — no drivers, no app, no pairing, and no LE Audio support required. The
UAC2 class callbacks in `app/src/input/input_usb_uac2.c` hand received PCM to the same
`input_frame_ingress` path the BLE sink uses, re-chunked to a stable frame size so the I2S
backend is not forced to reconfigure per USB frame.

## SOF integration seam

SOF is optional and can be integrated by adding a new backend or pipeline adapter without changing input adapter contracts.
