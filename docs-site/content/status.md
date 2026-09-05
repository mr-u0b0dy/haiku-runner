---
title: Project Status
---

What actually works, what is built but unproven, and what is still a stub. "Verified" here means observed working on real `nrf5340dk/nrf5340/cpuapp` hardware, not merely compiling.

## Verification matrix

| Subsystem | State | Evidence |
| --- | --- | --- |
| Dual-core BLE bring-up (net-core `hci_ipc`) | **Verified** | Boot log shows HCI transport IPC, controller identity, advertising |
| LE Audio unicast sink (PACS/ASCS/CAS/VCS/TMAS) | **Partly verified** | A phone bonds and discovers all five services; no audio stream ever established |
| USB Audio Class 2 input | **Verified end to end** | Enumerates as an ALSA card; tone confirmed audible from the amplifier |
| I2S output backend | **Verified end to end** | Same test; 70 s continuous playback with zero underruns |
| I2S self-test melody | **Verified** | Pre-existing bring-up path |
| Bond persistence (NVS) | **Verified** | `fs_nvs` init in log; re-pairing no longer fails |
| AUX analog input (SAADC) | **Partly verified** | Capture runs and frames reach the backend; no analog front-end built, so no audio confirmed |
| Source button + LEDs + audible cue | **Unverified** | Initialises (`source control ready`), but no button press has been tested |
| Source arbitration / auto-fallback | **Unverified in practice** | Logic exercised only incidentally; never deliberately tested with two live sources |
| Wi-Fi input | **Stub** | Not possible on this board — the nRF5340 has no Wi-Fi radio |
| SOF pipeline adapter | **Seam only** | Compiles behind `CONFIG_HR_PIPELINE_SOF`; no implementation |

## The honest summary

**USB audio is the path that demonstrably works.** Plug the board's nRF USB connector into a PC or a phone in OTG mode and it behaves as a USB sound card, with audio coming out of the MAX98357A.

**LE Audio is complete on the device side but has never streamed**, because no LE Audio-capable source was available to test against: the phone used (iQOO Z5, Android 13) does not support LE Audio unicast, and this development PC's Bluetooth adapter is an Intel 9460/9560, which is Bluetooth 5.1 — LE Audio requires 5.2+ for isochronous channels. See [Known Issues](/known-issues).

**AUX captures real samples but needs external hardware** before it can be judged. See [AUX Jack Wiring](/test-hardware/aux-jack-wiring).

## Build configurations

All of these are known to build:

| Configuration | Flags |
| --- | --- |
| Default (silent) | *none* — stub backend, BLE only |
| BLE speaker | `HR_BACKEND_I2S=y` |
| USB speaker | `HR_BACKEND_I2S=y HR_INPUT_USB=y HR_DEFAULT_INPUT_USB=y` |
| AUX speaker | `HR_BACKEND_I2S=y HR_INPUT_AUX=y HR_DEFAULT_INPUT_AUX=y` |
| All inputs | `HR_BACKEND_I2S=y HR_INPUT_USB=y HR_INPUT_AUX=y` |
| USB without hardware | `HR_INPUT_USB=y HR_INPUT_USB_UAC2=n HR_INPUT_USB_MOCK_FEEDER=y` |
| Amp bring-up | `HR_BACKEND_I2S=y HR_BACKEND_I2S_TEST_TONE=y` |

Note the default build uses the **stub** backend, so a plain `west build` with no flags produces a device that runs but makes no sound. This is deliberate — the stub keeps the pipeline testable without hardware — but it surprises people.
