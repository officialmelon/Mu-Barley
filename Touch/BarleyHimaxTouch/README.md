# Barley HX83102J touchscreen

This package exposes the Lenovo TB330XU's physical Himax HX83102J touch
controller to Windows as a standard ten-contact HID touchscreen.

The hardware contract is derived from Lenovo's live device tree and stock
Android driver rather than from a fabricated HID-over-I2C device:

- HX83102J zero-flash controller (`himax,hxcommon`)
- MT6768 SPI0, chip select 0, SPI mode 1, maximum 10 MHz
- reset GPIO 92
- native coordinates 0..11999 by 0..19199
- ten native contact slots
- vendor firmware `Himax_firmware_boe.bin`, 261120 bytes

## Architecture

- `mtk_spi.c` is the polling PIO transport for the MT6765/MT6768 SPI block.
- `himax_hx83102j.c` implements the Himax register protocol, zero-flash
  firmware upload, hardware CRC verification, and native event parsing.
- `barley_touch.c` translates native events to the mandatory Windows
  touchscreen usages and publishes them through Microsoft's Virtual HID
  Framework (VHF).

The source is deliberately layered so the initial PIO transport can later be
replaced by a reusable MediaTek SpbCx controller without changing the Himax or
HID translation layers.  Polling is used for first hardware validation; the
real GIC/EINT resources are known and can replace polling after the transport
is proven.

`ACPI\\BAR0002` supplies four versioned MMIO resources in order: SPI0,
TOPCKGEN, INFRACFG_AO, and GPIO.  Physical addresses are not compiled into the
Windows driver.

The Lenovo firmware blob is proprietary device data.  It is copied into the
test package by `Build-Arm64.ps1` and is not part of the portable source.

## Build

```powershell
.\Build-Arm64.ps1 -Configuration Release
```

The script validates the exact stock firmware size and SHA-256, builds an
ARM64 KMDF driver with the installed WDK libraries, links `vhfkm.lib`, and
runs `Inf2Cat` over the complete package.
