# Build

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

## Common config toggles

- Enable AUX adapter: `CONFIG_HR_INPUT_AUX=y`
- Enable USB-C adapter: `CONFIG_HR_INPUT_USB_C=y`
- Enable Wi-Fi adapter: `CONFIG_HR_INPUT_WIFI=y`
