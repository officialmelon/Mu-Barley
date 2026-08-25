# MT6768 MSDC runtime contract and remaining gates

This file separates facts already proved by Barley's firmware from behavior
that still needs a Windows-side hardware test.

## Why SDPORT, not StorPort

Windows already supplies the SD/MMC protocol stack in `sdport.sys`,
`sdbus.sys`, and `sdstor.sys`.  Microsoft's host-controller sample registers a
small hardware miniport with `SdPortInitialize`; that is the correct boundary
for MediaTek's non-SDHCI register interface.  StorPort would require this
project to reimplement disk/media semantics that the SD stack already owns.

The stock WinPE ARM64 image includes the three Microsoft SD runtime drivers,
but none of its INFs bind to a MediaTek MSDC controller.  It can therefore use
this miniport only after ACPI exposes a truthful vendor device and the driver
package is present in the image.

## Barley host1 resources

| Resource | Proven value | Source |
| --- | --- | --- |
| Main MMIO | `0x11240000 + 0x10000` | live Barley FDT / MT6768 platform library |
| MSDC-TOP MMIO | `0x11C90000 + 0x1000` | live Barley FDT / MT6768 platform library |
| Interrupt | SPI 101, Windows GSI 133 | live Barley FDT |
| Data width | 4 | live Barley FDT / UEFI PCD |
| Card detect | external MTK GPIO 18 | UEFI PCD and MT6768 implementation |
| Rails | VMCH + VMC | MT6768 `MsdcImplLib` |
| Safe bring-up rate | no more than 25 MHz | Barley UEFI policy |

Only the main MMIO and GSI are exposed by the diagnostic SSDT.  The miniport
does not pretend that the second MMIO window is part of SDPORT's one-base
initialization callback.  It instead preserves the MSDC-TOP tuning and clock
state left by LK/UEFI for the first enumeration test.

## Boot-critical interfaces

For a Windows Setup test the package requires all of the following:

1. ACPI device `ACPI\MTK6768`, with the main register resource first and GSI
   133 second.
2. `mtkmsdc.sys` as an SDHost-class boot-start service in the WinPE image.
3. Microsoft's `sdport.sys`, `sdbus.sys`, and `sdstor.sys`, already present in
   the audited ARM64 WinPE image.
4. LK/UEFI leaving VMCH/VMC, pinmux, controller clock, and MSDC-TOP state live
   across `ExitBootServices()`.

The current driver intentionally reports inserted and write-protected media.
That makes `diskpart list disk` or Setup disk enumeration the pass criterion;
Windows installation to the card is not yet a claimed or safe use case.

## API and hardware validation gates

The code is an implementation skeleton until these are resolved:

- **SDPORT completion contract:** Microsoft documents and samples asynchronous
  request completion.  This diagnostic implementation polls at DISPATCH_LEVEL
  and returns a terminal NTSTATUS.  That path must be validated against the
  target WDK/sdport build before deployment; if sdport requires pending-only
  completion, the polling engine must move behind `RequestDpc` without changing
  the hardware code.
- **R2 response byte order:** the MTK response registers and SDPORT's expected
  136-bit buffer layout need a card-identification trace on Windows.
- **Clock parent rate:** `320 MHz` matches Barley's currently inherited
  firmware configuration but is not owned by this driver.  A production stack
  needs an MT6768 clock/PMIC/pinctrl contract rather than relying on handoff.
- **Card detect:** GPIO18 cannot be read through `MSDC_PS`; a production driver
  needs an MT6768 GPIO/GpioClx resource and debounce path.
- **Power transitions:** D-states, cold boot, reboot, resume, and hot-plug are
  unsupported until rail, clock, pinmux, and MSDC-TOP ownership is implemented.
- **Write path:** deliberately absent.  It must not be enabled before reads,
  resets, error recovery, and removal behavior are stable.

## Ranked blockers after the first read-only enumeration test

1. Runtime ownership of MSDC1 clocks, MSDC-TOP, pinmux, VMCH, and VMC.
2. Correct asynchronous SDPORT completion and multi-block transfers.
3. External GPIO18 card detect and removal handling.
4. Safe write support and cache/flush semantics.
5. eMMC host0 enablement, including boot partitions, reset, HS200/tuning, and
   strict Android GPT protection during bring-up.

The SD card is a useful source medium, but Windows may still treat removable
SD media as an unsupported installation target.  The eventual OS target is
normally eMMC host0; exposing that controller must wait until the same runtime
driver has proved safe read-only behavior.
