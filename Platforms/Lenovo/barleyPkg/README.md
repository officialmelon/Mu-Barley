# Lenovo Tab M11 (TB330XU / Barley)

This package is the Mu-Silicium bring-up port for the Lenovo Tab M11
LTE, model TB330XU, board `barley_row_lte`. Android's `mt8786` hardware name is
part of the MT6768/MT6769 BSP family, so the platform deliberately reuses
`Silicon/MediaTek/MT6768Pkg` rather than introducing a parallel silicon package.

No Lenovo or MediaTek proprietary firmware is stored in this repository.

## Build

From the repository root:

```text
python build_uefi.py -d barley -r RELEASE
```

On Windows, install the GnuWin32 Make required by Mu's build documentation.
The Barley build script prefers its standard installation directory and avoids
passing Windows paths through an unrelated MSYS shell when both are installed.

The build produces the 2 MiB firmware device at
`Build/barleyPkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd`, compiles the upstream
ARM64 `BootShim`, appends the FD to that executable shim, gzip-compresses the
combined payload, and places it in an Android boot-image v4 kernel field. The
result is `Mu-barley.img`. A raw FD by itself is not a valid LK kernel payload.
Do not flash build artifacts automatically.

## Hardware evidence and provenance

| Constant or decision | Source | Status |
| --- | --- | --- |
| MT6768 silicon package | Android `ro.board.platform`, Barley DT compatibles, MMIO comparison | VERIFIED |
| `0x40080000` LK kernel entry / BootShim base | Physical LK execution log | VERIFIED |
| `0x4BD00000` relocated FD base | M2.7/M2.8 physical execution and residency tests | VERIFIED |
| 2 MiB FD size | Existing MT6768/lancelot firmware layout | REUSED MT6768 CONSTANT |
| GICD `0x0C000000`, size `0x40000` | Barley DT and MT6768 package | VERIFIED MATCH |
| GICR `0x0C040000`, size `0x200000` | Barley DT and MT6768 package | VERIFIED MATCH |
| PMIC wrapper `0x1000D000` / MT6358 | Barley DT/runtime and MT6768 package | VERIFIED MATCH |
| MSDC0 `0x11230000`, size `0x10000`, GSI 132 | Barley DT and MT6768 package | VERIFIED MATCH |
| MSDC1 `0x11240000`, size `0x10000`, GSI 133 | Barley DT and MT6768 package | VERIFIED MATCH |
| MSDC0 Top `0x11CD0000`, size `0x1000` | Live LK FDT `/mmc@11230000/reg` and `/proc/iomem` | VERIFIED |
| MSDC1 Top `0x11C90000`, size `0x1000` | Live LK FDT `/mmc@11240000/reg` and `/proc/iomem` | VERIFIED |
| eMMC GPIO 122-133 mapping | Barley DT and MT6768 `MsdcImplLib` | VERIFIED MATCH |
| microSD GPIO 161-164, 170-171 mapping | Barley DT and MT6768 `MsdcImplLib` | VERIFIED MATCH |
| logo decompression output `0x7A3F8000`, size `0x008CA000` | Physical LK expdb (`out`, `have`) | VERIFIED |
| OVL framebuffer `0x7BCE0000`, size `0x01F20000` | Physical LK expdb and reserved-memory | VERIFIED |
| distinct display allocation `0x7E605000`, size `0x017E8000` | Lenovo reserved-memory evidence | VERIFIED |
| native panel 1200 x 1920 | active `hx83102j_dsi_vdo_boe` selection and runtime display | VERIFIED |
| OVL format `eBGRA8888`, live pitches 4800/4864 | Physical LK expdb layer configuration | VERIFIED |
| stack `0x40000000..0x4003FFFF` | Both live LK `mblock_info` captures, mblock 0 | VERIFIED STABLE |
| DXE heap `0x56000000..0x6BFFFFFF` | Both live LK `mblock_info` captures, mblock 7 | VERIFIED STABLE |
| one continuous 8 GiB region from `0x40000000` | No valid source; conflicts with dynamic mblock carveouts | INVALID — NOT USED |

## Live-FDT memory gate

Two independently captured LK-patched FDTs establish physical RAM as
`0x40000000 + 0x200000000`. Their `/memory/reg`, all 23 fixed
`/reserved-memory/*/reg` entries, and complete 22,552-byte MediaTek
`/memory/mblock_info` properties are byte-identical:

| Capture | SHA-256 |
| --- | --- |
| `live-barley.dtb` | `A09ED364C2E98E3E08CBB56D9AD7EC486007486833A0BA25E05E616BE607CE4A` |
| `live-barley-2.dtb` | `4DD7BB763DE6EED898E2D085DEF7D29308FF37D6F8B48C6DCF5DE6C039E40070` |

`MemoryMapLib` validates the live LK FDT in place at the verified DTB handoff
address `0x4BC80000`. It checks the FDT header and size, parses `/memory/reg`,
walks every `/reserved-memory` child `reg`, observes `no-map`, and rejects the
broad memory map if the expected topology or any reservation-to-mblock
relationship changes. Free descriptors are limited to mblocks stable in both
captures. The gaps preserve ATF, TEE, GZ, SCP, SSPM, modem/CCCI, pstore,
DRAMC-tail, boot-image, DTB, and other LK allocations.

The former provisional `0x40280000 + 0x03C00000` heap is gone. SEC/DXE uses
stable mblock 7 at `0x56000000 + 0x16000000`. If live-FDT validation fails, the
implementation fails closed and does not add the other broad mblocks.
`RamManagerDxe` remains excluded because it would flatten the physical 8 GiB
range into one unsafe allocation.

`BarleyLkGopDxe` wraps the display pipeline already configured by Lenovo LK; it
does not initialize DSI, reset the panel, change clocks/timings, or access
display MMIO. LK expdb proves an OVL direct-link handoff with `eBGRA8888`,
1200 x 1920 layers. Layer 0 alternates between `0x7BCE0000` and `0x7CEB0000`
at a 4864-byte pitch, while the full-screen layer-3 surface at `0x7C5C8000`
uses a 4800-byte pitch. All three live inside the verified framebuffer
carveout and the aligned allocations are spaced by `0x008E8000`.

Register readback after LK's handoff does not identify the composited buffer
pool reliably enough to gate GOP installation. The passive GOP therefore
mirrors every BLT to all three verified surfaces with each surface's own
stride. Its public mode is `PixelBltOnly`, and its direct BLT routines force
the BGRA alpha byte to `0xFF` for fills and buffer-to-video writes. The distinct
LK logo decompression allocation at `0x7A3F8000` and FDT display reservation at
`0x7E605000` are not treated as scanout. `LK Framebuffer`, `LK Logo Surface`,
and `Display Reserved` remain separate reserved descriptors. LK free mblock
11 remains omitted.

Buttons are intentionally omitted. Android evidence shows power and
volume-down on `mtk-pmic-keys`, while volume-up is on `mtk-kpd`. Lancelot's
GPIO-93 volume-down assignment does not apply, and no GPIO/keyscan value is
invented here.

The first storage milestone uses the existing MT6768 GPIO, clock, PMIC-wrapper,
MT6358, MSDC, and EMMC drivers to expose only eMMC hardware Block I/O. Lenovo's
live FDT pairs host 0 with `msdc0_top@11cd0000` and host 1 with
`msdc1_top@11c90000`; preserving those associations is required because
`MsdcDxe` looks the controller and Top descriptors up independently by host
index. `EmmcDxe` is connected only to handles exposing the MediaTek SD/MMC
pass-through protocol before the internal Shell starts. Partition, FAT, and SD
drivers remain excluded until physical Block-I/O enumeration is proven. No
storage data is formatted or repartitioned by this milestone.

Initial ACPI reuses the MT6768 single-core MADT, generic timer, GIC, minimal
DSDT, FADT, and common SSDT. Multicore PSCI/MADT work is deferred until a stable
single-core boot exists.

## Reused MT6768 components

The port reuses `GpioImplLib`, `ClockImplLib`, `PmicWrapperImplLib`, and
`MsdcImplLib`. Barley overrides only `PlatformSecLib`: Lenovo LK has already
entered at EL1, so its assembly initializer is a no-op and its C initializer
only disables TOPRGU. This avoids the Lancelot-specific OVL mutation in the
shared MT6768 library. The core shell and passive GOP are physically proven;
additional peripherals are now introduced one dependency chain at a time.

## M2 execution gate

The two-capture memory-map gate is complete. Before controlled execution,
verify framebuffer format/stride. The permanent packaging configuration uses
the upstream `BootShim.bin + SILICIUM_UEFI.fd` layout, gzip compression matching
the Lenovo kernel flow, and Android boot header v4. LK starts the shim as its
kernel; the shim copies the appended 2 MiB FD to `0x4BD00000` and branches to
it while preserving incoming `x0`. A Barley device boot-manager library launches
the FV-resident internal Shell after GraphicsConsole connects to
`BarleyLkGopDxe`.
Preserve Lenovo preloader, ATF, TEE, LK, GPT, vendor_boot, and slot B. Physical
testing is restricted to the already-proven `boot_a` test / `boot_b` recovery
flow on the unlocked device.
