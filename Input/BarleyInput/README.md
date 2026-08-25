# Barley physical-key Windows HID transport

`BarleyInput.sys` is an ARM64 KMDF HID minidriver for the Lenovo Tab M11
TB330XU (`barley_row_lte`). It is a Windows kernel input device, not an EFI
key translation, so it remains available after `ExitBootServices()`.

The driver follows Microsoft's `hid/vhidmini2` KMDF architecture and binds to
the real ACPI device `ACPI\BAR0001`. `MsHidKmdf.inf` supplies the HID-class
function driver and this driver is its lower hardware transport filter.

## Keys

| Hardware source | Windows HID report |
| --- | --- |
| MT6768 KPD `KP_MEM1[0]`, active low | Tab |
| MT6358 `TOPSTATUS.HOME`, active low | Shift+Tab |
| MT6358 `TOPSTATUS.PWRKEY` tap | Enter |
| MT6358 power hold (500 ms) | Escape |

The 20 ms poller reports both make and break transitions. The standard boot
keyboard report descriptor is deliberately small and contains no vendor
feature protocol.

## ACPI/resource contract

The driver does not contain physical base addresses. It parses the translated
memory resources assigned by ACPI. `BAR0001` version 1 defines them in order:

1. MT6768 KPD, 4 KiB
2. MT6768 PWRAP, 4 KiB

`BarleyInput.asl` documents that contract. The matching node is already present
in Barley's firmware DSDT. Register offsets remain hardware ABI constants.

The PWRAP path is read-only at the PMIC level. The only MMIO writes are the
required WACS2 read command and WACS2 valid-clear acknowledgement. All WACS
waits are bounded to 10 ms; a failed PMIC read disables only PMIC-derived keys,
while KPD Tab remains functional.

## Build

From PowerShell:

```powershell
cd C:\Users\braxt\TB330XU\Barley-Windows-Drivers\Input\BarleyInput
.\Build-Arm64.ps1 -Configuration Release
```

The script uses the locally unpacked Microsoft ARM64 WDK NuGet package and the
available LLVM `clang-cl`/`lld-link` toolchain. Output is under
`out\ARM64\Release`; the ready-to-sign INF/SYS/CAT set is in its `package`
subdirectory. The script runs `Inf2Cat`, prints the PE/COFF machine, and hashes
both the driver and unsigned catalog.

## Test signing and WinPE

This prototype must not be loaded unsigned. For a development-only image:

1. Create a private test code-signing certificate.
2. Use the generated `package\BarleyInput.cat` (built with the WDK's current
   ARM64 OS identifier, `10_GE_ARM64`).
3. Sign both the catalog and SYS with SHA-256 and that certificate.
4. Add the certificate to the offline WinPE `ROOT` and `TrustedPublisher`
   stores.
5. Enable `testsigning` for the WinPE BCD entry only while Secure Boot is off.
6. Inject the complete package with `dism /image:<mounted boot.wim index> /add-driver /driver:BarleyInput.inf`.

Do not disable integrity checks on a production installation. Replace the test
signature with an attestation/WHQL-signed package before distribution.

## Scope

This provides the three physical-key navigation actions required to operate
WinPE Setup. It does not claim touchscreen, USB host, audio, or storage support.
Those are separate Windows device drivers and belong in sibling source trees.
