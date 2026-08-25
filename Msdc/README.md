# MT6768 MSDC Windows driver bring-up

This directory is intentionally separate from Mu-Silicium.  It contains the
Windows runtime storage work needed after `ExitBootServices()`; it does not
change the UEFI SD/MMC driver or any boot media.

## Architecture

`mtkmsdc.sys` is a vendor-specific SDPORT host-controller miniport for the
MediaTek MT6768 MSDC register block.  It is **not** an SDA/SDHCI controller and
must not be described as `PNP0D40`.

The first diagnostic target is only the removable microSD controller:

| Item | Barley value |
| --- | --- |
| ACPI hardware ID | `ACPI\MTK6768` |
| MSDC host | 1 |
| Controller MMIO | `0x11240000`, length `0x10000` |
| GIC interrupt | GSI 133, level/high |
| Bus width | 4-bit |
| Maximum diagnostic clock | 25 MHz requested |
| Data path | PIO, one block at a time |
| Media policy | reported write-protected |

The diagnostic miniport deliberately exposes no eMMC device and rejects every
data-write request with `STATUS_MEDIA_WRITE_PROTECTED`.  This allows Windows
Setup to prove that its post-UEFI storage stack can enumerate and read the SD
card without risking the Android GPT or either boot slot.

## What is implemented

- ARM64-only WDM/SDPORT project and INF.
- Vendor ACPI binding (`MTK6768`), with no false `PNP0D40` compatible ID.
- SDPORT callback table based on Microsoft's standard SDHC sample.
- MT6768 command encoding, response capture, reset, FIFO handling, clock
  divider, bus width, interrupt decoding, and synchronous PIO reads based on
  Mu-Silicium's proven `MsdcDxe` implementation.
- A host1-only ACPI SSDT fragment.
- Hard read-only policy and a one-request/one-block capability limit.

## Intentional first-test limitations

This is a concrete bring-up implementation, not yet a production driver:

1. The slot rail, top clock gates, pinmux, and MSDC-TOP tuning are inherited
   from LK/UEFI.  The current UEFI driver leaves them enabled at
   `ExitBootServices()`.  Resume, cold Windows restart, voltage switching, and
   hot-plug therefore remain unsupported.
2. Barley uses external GPIO18 card detect.  Until a Windows MT6768 GPIO driver
   exists, this host1-only build reports the already-inserted card as present.
3. The port uses synchronous polled completion and limits transfers to one
   block.  Interrupt-driven completion and multi-block I/O are the next step
   after enumeration is proven.
4. Writes are disabled by design.  Do not use this build as an installer target.
5. `CrashdumpSupported` is false.  Hibernation/crash-dump storage is not yet
   claimed.

## Build prerequisites

Install Visual Studio with the ARM64 C++ tools and the Windows 11 WDK.  The
plain Windows SDK is insufficient: the project requires `sdport.h` and
`sdport.lib` from the kernel-mode WDK.

Build from a Developer PowerShell:

```powershell
msbuild .\mtkmsdc.vcxproj /m /p:Configuration=Debug /p:Platform=ARM64
```

The currently installed host SDK was audited as lacking the WDK `km` include
and library trees.  Microsoft's official ARM64 WDK 26100 package does contain:

- `Include/10.0.26100.0/km/sdport.h`
- `Lib/10.0.26100.0/km/ARM64/sdport.lib`

## Safe first runtime test

After the driver compiles and is test-signed, the controlled test is:

1. Add only the host1 ACPI node to a test firmware.
2. Inject the signed package into both WinPE/Setup indexes of `boot.wim`.
3. Leave host0/eMMC absent from ACPI.
4. Boot WinPE and confirm that one read-only SD disk appears in Setup or
   `diskpart list disk`.
5. Capture SetupAPI and kernel debug logs before enabling writes.

Do not inject or deploy this skeleton until it builds cleanly and its signing
policy is decided.  Nothing in this directory performs deployment.

## Primary references

- [Microsoft standard SD host-controller miniport sample](https://github.com/microsoft/Windows-driver-samples/tree/main/sd/miniport/sdhc)
- [Microsoft SDPORT initialization DDI](https://learn.microsoft.com/en-us/previous-versions/mt715818%28v%3Dvs.85%29)
- [Microsoft ACPI requirements for SoC SD controllers](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/other-acpi-namespace-objects#sd-host-controllers-and-devices)
- [Linux MediaTek MMC host driver](https://github.com/torvalds/linux/blob/master/drivers/mmc/host/mtk-sd.c)
- Mu-Silicium source: `Silicon/MediaTek/MediaTekPkg/Drivers/MsdcDxe/`
- Mu-Silicium source: `Silicon/MediaTek/MT6768Pkg/Library/MsdcImplLib/`

