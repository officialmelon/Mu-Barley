# TB330XU M2.62 engineering handoff

Updated: 2026-08-29 (Australia/Brisbane)

## Repository layout

This GitHub repository contains two independent histories:

- `main`: Mu-Silicium/EDK2 firmware for Lenovo Tab M11 TB330XU (`barley_row_lte`).
- `windows-drivers`: ARM64 Windows drivers for physical keys, HX83102J touch, and the MT6768 MSDC SDPORT miniport.

The latest relevant commits are:

- Firmware: `400b9e64` (`mediatek: negotiate MSDC width and quiesce OS handoff`).
- Windows drivers: `6f32bd9` (`barley: harden input touch and SDPORT lifecycle`).

The two branches have unrelated Git histories by design. Do not merge `windows-drivers`
into `main`; inspect them independently.

## Objective

Boot a usable ARM64 Windows PE/installer environment while retaining an untouched
Android recovery slot. The current milestone is read-only enumeration:

1. MT6768 MSDC ACPI devices bind and start.
2. Card identification succeeds.
3. SDPORT creates child disk devices.
4. `diskpart` displays disks and volumes without partitioning or formatting.
5. Touch and physical keys remain functional before and after storage starts.

## Latest physical M2.62 result

M2.62 was built, injected into WinPE index 2, hashed, staged to the microSD, and
booted from `boot_a`.

Observed on the tablet:

- Touch worked after a full power-off/cold boot.
- Windows PE subsequently crashed (the user may have triggered an unrelated action).
- On a warm retry, touch reported `CM_PROB_FAILED_START` / Code 10.
- After fully powering the tablet off and on, touch worked again.
- No fixed disks were shown.
- No volumes were shown.
- `diskpart` appeared to pause for roughly 20 seconds immediately after opening,
  then returned empty disk/volume lists.

Interpretation:

- The touch hardware can initialize with the current driver after a clean hardware
  reset, so ACPI matching, SPI access, firmware presence, and the basic initialization
  path are substantially correct.
- The warm-restart Code 10 is probably residual HX83102J/controller state or an
  incomplete cleanup/recovery path. It is not evidence that initialization must occur
  only once permanently. A full loss of power resets the hardware state that the warm
  path currently fails to recover from.
- The approximately 20-second `diskpart` delay is likely meaningful. It is consistent
  with the storage stack waiting for one or more device/command timeouts. The host
  reporting `Started` is not proof that card identification or child enumeration works.
- The primary unresolved problem remains the Windows SDPORT miniport request/response
  contract, command completion, or MT6768 controller/card initialization. Do not return
  to unrelated touchscreen feature work until storage enumeration is understood.

## M2.62 firmware changes (`main`)

Relevant files:

- `Silicon/MediaTek/MediaTekPkg/Drivers/MsdcDxe/MsdcDxe.c`
- `Silicon/MediaTek/MediaTekPkg/Drivers/MsdcDxe/MsdcDxe.h`
- `Silicon/MediaTek/MediaTekPkg/Drivers/MsdcDxe/MsdcDxe.inf`
- `Silicon/MediaTek/MediaTekPkg/MediaTekPkg.dec`
- `Platforms/Lenovo/barleyPkg/barley.dsc`

Implemented behavior:

- SD 4-bit mode is negotiated card-side with ACMD6 before switching the host width.
- eMMC 8-bit mode is negotiated through EXT_CSD before switching the host width.
- Negotiation failure falls back safely to 1-bit.
- Native MediaTek 136-bit/R2 response order is `RESP3`, `RESP2`, `RESP1`, `RESP0`.
- The previous SDHCI-style R2 byte compaction was removed.
- An ExitBootServices callback returns the card/host to a conservative Windows handoff:
  card reset/idle, host 1-bit, controller reset, FIFO clear, interrupt clear, and
  400 kHz clock. Rails, parent clocks, and pinmux remain available.
- Board PCDs describe eight wired eMMC data lines and four wired microSD data lines.

The firmware still needs to be checked for exact handoff interaction with Windows,
but the Windows miniport must not assume UEFI left a fully enumerated card alive.

## M2.62 Windows-driver changes (`windows-drivers`)

### MSDC/SDPORT

Relevant files:

- `Msdc/mtkmsdc.c`
- `Msdc/mtkmsdc.h`
- `Msdc/mtkmsdc.inx`
- `Msdc/acpi/mtkmsdc1.hex`

Implemented behavior:

- Advertised AutoCMD12 support is disabled.
- Initialization explicitly selects 400 kHz.
- SDIO mode is not enabled for memory-only hosts.
- Native MediaTek R2 response ordering replaces SDHCI-style compaction.
- Only one outstanding SDPORT request is accepted.
- Overlapping controller commands are rejected.
- A started command returns `STATUS_PENDING` exactly once.
- Command completion occurs from the request DPC.
- PIO processing validates request ownership.
- Transfer completion waits for the latched transfer-complete indication.
- Synchronous `SdPortCompleteRequest` from `IssueRequest` was removed.

Package version: `0.6.0.0`.

The next audit must compare every callback and request phase against Microsoft's
non-SDHCI SDPORT miniport contract, not merely against Linux or UEFI register logic.
Pay particular attention to the command sequence before the 20-second `diskpart`
delay, response types, event masks, timeout recovery, transfer setup ordering, and
whether SDPORT expects a card-detect/state-change notification before issuing card
identification commands.

### HX83102J touch

Relevant files:

- `Touch/BarleyHimaxTouch/barley_touch.c`
- `Touch/BarleyHimaxTouch/barley_touch.h`
- `Touch/BarleyHimaxTouch/himax_hx83102j.c`

The vendor safe-mode password is now sent as two separate one-byte writes:

- `0x27` to command/register `0x31`.
- `0x95` to command/register `0x32`.

The path waits for central state `0x0C`, then performs the vendor TCON and ADC reset
sequence. Cold boot now proves the path can work. A later improvement should make
prepare/release hardware and failed-start recovery idempotent so a warm restart can
recover without requiring full device power removal.

### Physical keys

The deployed WinPE deliberately used the exact earlier M2.56.2 signed physical-key
package because that package had physically worked. Do not assume the newest source
build was the binary injected into M2.62. Preserve this distinction when comparing
source commits, package versions, and WIM contents.

## Built artifact identities

The large WIM and boot image are intentionally not committed to GitHub.

M2.62 WinPE WIM:

- Size: `469592197` bytes.
- SHA-256: `C99118EB89AFEB6C14D180F7BFDDF252FFD5FF501ADB5541C26B47175024C24A`.
- The SD copy at `sources/boot.wim` was hashed after staging and matched.

M2.62 UEFI Android boot image:

- Size: `33554432` bytes.
- SHA-256: `0C59F242CB09B5D1B0E7E6AC9B83FA777114880191BE1B6066BC7A6399942E90`.
- It was flashed only to `boot_a`.

M2.62 storage driver:

- SYS SHA-256: `B71ADDADB736AFDCA0CB1938C4B7367B9C8CF74A2F1C9BC7B12FE3588D3BA961`.
- CAT SHA-256: `F3E47DA6DFD94D4EFDAE74DAF8B0FC2A50BE0ECF366C754512FB48DF77B9689D`.

M2.62 touch driver:

- SYS SHA-256: `0C7608D355E7BCFF2D51EACEDFDA45E8981BF83CEFAFD6A33767E8C96790736E`.
- CAT SHA-256: `D81CE60E1C62B8A6D095B1BD51FE8883BB936230BB048D11F75DD5E53F125DCA`.

HX83102J firmware:

- Size: `261120` bytes.
- SHA-256: `58AE7D487EF16A43DE068A85DED0913E471F4AB806C053EA76611E3B1B21353D`.

## Build requirements

Use native Microsoft ARM64 tooling:

- Visual Studio ARM64 `cl.exe` and `link.exe`.
- WDK NuGet `10.0.26100.6584-arm64`.
- Kit target `10.0.26100.0` / `10_GE_ARM64`.
- KMDF `1.33` for input/touch.

Compilation alone is insufficient. Verify ARM64 PE machine, native subsystem and
entry point, imports, Inf2Cat, SYS/CAT signatures, exact WIM contents, WIM index 2,
WIM verification, and the final hash staged to the SD card.

## Diagnostic constraints

Logs under `X:` cannot be collected after the test, so diagnostics must be concise
and visible on screen for photography. The WinPE script must not depend on `findstr`,
which is absent from this image.

Storage diagnostics must remain read-only:

- Allowed: PnP enumeration, registry queries, `diskpart` `list disk`, and
  `diskpart` `list volume`.
- Forbidden during enumeration debugging: `clean`, partition creation/deletion,
  formatting, installation writes, or erasing any disk.

Useful next on-screen instrumentation would show bounded counters and the last command:

- last SD command index and argument;
- response type and four response registers;
- requested/observed event masks;
- command and transfer interrupt status;
- timeout stage;
- card-present/card-detect state;
- request phase and whether completion was issued;
- current host clock and bus width.

Expose these through service registry diagnostics or another screen-readable mechanism;
do not add blocking debug output inside interrupt/DPC paths.

## Fastboot safety state

Before the M2.62 test:

- Product: `barley_row_lte`.
- Bootloader unlocked: yes.
- Slot A: not successful, not unbootable; designated test slot.
- Slot B: successful and bootable; untouched Android recovery slot.

Rules:

- Never erase, format, or repartition through fastboot.
- Never flash `super`, `system`, `vendor`, `userdata`, `metadata`, `vbmeta`, or the
  bootloader as part of this investigation.
- Flash only an explicitly authorized boot slot.
- Preserve slot B unless the user explicitly changes the recovery plan.
- Verify slot state before rollback. The intended rollback, only after verification,
  is `fastboot set_active b` followed by `fastboot reboot`.

## Most useful next research question

Given a MediaTek MT6768 non-SDHCI Windows SDPORT miniport that binds and reports
Started but creates no child disks, and causes approximately a 20-second delay when
`diskpart` opens, identify the exact SDPORT callback/request sequence and completion
rules required for initial SD/eMMC enumeration. Compare the `windows-drivers` branch
against current Microsoft non-SDHCI SDPORT documentation/samples and upstream MT6768
MSDC command/interrupt behavior. Produce a concrete patch plan tied to functions and
request phases, with special attention to card-detect notification, CMD0/CMD8/ACMD41
or CMD1 sequencing, R2 response layout, PIO preparation, timeout cancellation, and
exactly-once completion.

