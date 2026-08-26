# MediaTek MT6768 MSDC Windows miniport

This directory contains the Windows runtime storage driver for Lenovo Barley.
It is separate from Mu-Silicium because UEFI block I/O stops at
`ExitBootServices()`.

`mtkmsdc.sys` is an ARM64 SDPORT host-controller miniport for MediaTek's
vendor-specific MSDC register interface. MSDC is not SDHCI-compatible and is
therefore exposed through the vendor ACPI ID `MTK6768`, never `PNP0D40`.

## Barley controllers

| ACPI device | Medium | Main MMIO | GSI | Width | Removal |
| --- | --- | --- | --- | --- | --- |
| `MSD0` / UID 0 | soldered eMMC | `0x11230000 + 0x10000` | 132 | 8-bit capable | fixed |
| `MSD1` / UID 1 | microSD | `0x11240000 + 0x10000` | 133 | 4-bit | removable |

One signed package binds to both instances. The miniport distinguishes them by
their translated physical register resource. It implements command/response
handling, reset and recovery, clock division, 1/4/8-bit bus widths, multi-block
PIO reads and writes, Auto CMD12, and SDPORT power/bus callbacks. The initial
clock policy deliberately stays at normal speed (25 MHz maximum); high-speed,
HS200 and UHS tuning are deferred until baseline I/O is measured on hardware.

## Hardware ownership contract

The first Windows version preserves the working state left by Lenovo LK and
Mu-Silicium: source clocks, controller gates, PMIC rails, pinmux and MSDC-TOP
tuning remain enabled across `ExitBootServices()`. The miniport owns the main
MSDC window and its local clock divider only. This is the smallest correct
bring-up boundary and matches the currently proven UEFI storage path.

MSDC1 card detect is wired to external GPIO18, not `MSDC_PS.CDSTS`. Until the
MT6768 GPIO/GpioClx driver exists, MSDC1 reports the card inserted and is tested
with the card present at boot. MSDC0 is permanently present. Neither controller
is falsely reported write-protected.

## Build

The reproducible build uses the checked local WDK NuGet package and the
installed ARM64 MSVC tools:

```powershell
.\Build-Arm64.ps1 -Configuration Release -SigningThumbprint <SHA1>
```

The output package is `out/ARM64/Release/package`. `Inf2Cat` validates the INF,
and the script emits PE headers and SHA-256 hashes.

## Current scope and next validation

The data path is conservative synchronous polling with one outstanding SDPORT
request and no DMA. This is intentional for first hardware enumeration and is
functionally sufficient for Setup, though slower than the eventual interrupt +
DMA path. Crash-dump/hibernation support, cold power ownership, resume, live
card removal, voltage switching and tuned high-speed modes are not claimed yet.

The controlled first test is read enumeration of both disks in WinPE, followed
by a small write/read/flush test on a disposable file on the microSD filesystem.
No automated test writes the eMMC or changes its GPT. Windows Setup writes only
after the user explicitly selects and confirms a destination.

## References

- Microsoft Windows driver samples: `sd/miniport/sdhc`
- Microsoft SDPORT and SoC ACPI documentation
- Linux `drivers/mmc/host/mtk-sd.c`
- Mu-Silicium `MediaTekPkg/Drivers/MsdcDxe`
- Mu-Silicium `MT6768Pkg/Library/MsdcImplLib`
