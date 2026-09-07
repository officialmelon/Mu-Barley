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
PIO reads and writes, and SDPORT power/bus callbacks. Auto CMD12 is deliberately
not advertised yet. The current clock is capped at 20 MHz; DMA, HS200, UHS, and
tuning are deferred until the conservative path is reliable enough to install
and recover Windows safely.

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

## Hardware-validated state

Driver `0.26.0.0` has enumerated both physical devices in Windows PE 26100.1:

- MT6768 MSDC0: 115 GiB-class eMMC, 8-bit, exposed through `sdstor`.
- MT6768 MSDC1: 119 GiB-class microSD, 4-bit, exposed through `sdstor`.

The package uses the standard Microsoft SDHC sample command-policy tables.
Responses, identities, capacities, and completion statuses come from hardware;
there are no card-specific CID/CSD replacements or success overrides. Run
`Test-PackageContract.ps1` after changes to check those invariants.

The Android eMMC GPT currently exposes 54 partitions/volumes to Windows. A
read-only DiskPart enumeration therefore generates tens of thousands of small
PIO requests. Sparse diagnostic export in `0.26.0.0` reduced the measured
DiskPart phase from about 238 seconds to 134 seconds, but this is still a
bring-up data path rather than production performance.

Package `0.27.0.0` retains that runtime code and adds the Microsoft SD-host
sample's `System Bus Extender` load-order group and boot-volume promotion flag.
This metadata is required before testing a full Windows boot from MSDC-backed
storage; enumeration alone does not validate that boot path.

## Current scope and next validation

The data path is conservative synchronous polling with one outstanding SDPORT
request and no DMA. It is sufficient for read-only enumeration, but Windows
installation and sustained I/O have not yet been validated. Crash-dump and
hibernation support, cold power ownership, resume, live card removal, voltage
switching, and tuned high-speed modes are not claimed yet.

Before installation, validate repeated cold-boot enumeration and read-only
access, then run a bounded write/read/flush test only on a disposable file in
the existing microSD FAT32 filesystem. No automated test writes the eMMC or
changes its GPT. Windows Setup must remain disabled until a destination and
rollback plan are explicitly approved.

`Decode-Trace.ps1` decodes a registry snapshot captured by the WinPE launcher.
It is diagnostic tooling and does not alter device state.

## References

- Microsoft Windows driver samples: `sd/miniport/sdhc`
- Microsoft SDPORT and SoC ACPI documentation
- Linux `drivers/mmc/host/mtk-sd.c`
- Mu-Silicium `MediaTekPkg/Drivers/MsdcDxe`
- Mu-Silicium `MT6768Pkg/Library/MsdcImplLib`
