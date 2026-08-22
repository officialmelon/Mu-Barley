# Lenovo Tab M11 (TB330XU / Barley)

This package is the conservative M1 Mu-Silicium port for the Lenovo Tab M11
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

M1 produces the 2 MiB firmware device at
`Build/barleyPkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd` and a raw convenience
copy named `Mu-barley.bin`. Boot-image construction and device execution are M2
work. Do not flash this M1 image.

## Hardware evidence and provenance

| Constant or decision | Source | Status |
| --- | --- | --- |
| MT6768 silicon package | Android `ro.board.platform`, Barley DT compatibles, MMIO comparison | VERIFIED |
| `0x40080000` payload / FD base | Lenovo vendor boot v4 header; matches lancelot | VERIFIED |
| 2 MiB FD size | Existing MT6768/lancelot firmware layout | REUSED MT6768 CONSTANT |
| GICD `0x0C000000`, size `0x40000` | Barley DT and MT6768 package | VERIFIED MATCH |
| GICR `0x0C040000`, size `0x200000` | Barley DT and MT6768 package | VERIFIED MATCH |
| PMIC wrapper `0x1000D000` / MT6358 | Barley DT/runtime and MT6768 package | VERIFIED MATCH |
| MSDC0 `0x11230000`, size `0x10000`, GSI 132 | Barley DT and MT6768 package | VERIFIED MATCH |
| MSDC1 `0x11240000`, size `0x10000`, GSI 133 | Barley DT and MT6768 package | VERIFIED MATCH |
| eMMC GPIO 122-133 mapping | Barley DT and MT6768 `MsdcImplLib` | VERIFIED MATCH |
| microSD GPIO 161-164, 170-171 mapping | Barley DT and MT6768 `MsdcImplLib` | VERIFIED MATCH |
| framebuffer base `0x7E605000` | Lenovo LK/boot framebuffer evidence | VERIFIED |
| framebuffer allocation `0x017E8000` | Lenovo reserved-memory evidence | VERIFIED |
| native panel 1200 x 1920 | active `hx83102j_dsi_vdo_boe` selection and runtime display | VERIFIED |
| 32-bit BGRX and 1200 pixels/scanline | Existing `SimpleFbDxe` contract | PROVISIONAL |
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

The framebuffer is exposed through the existing inherited `SimpleFbDxe` path;
M1 does not initialize DSI or the panel. The physical allocation is verified,
but pixel format, orientation, and pixels-per-scanline must be confirmed before
a display test. The current 1200 x 1920 x 32 configuration is explicitly
provisional. Actual scanout at `0x7E605000 + 0x017E8000` remains distinct from
`mblock-17-framebuffer` at `0x7BCE0000 + 0x01F20000`. LK free mblock 11 is
omitted because it overlaps the independent scanout allocation.

Buttons are intentionally omitted. Android evidence shows power and
volume-down on `mtk-pmic-keys`, while volume-up is on `mtk-kpd`. Lancelot's
GPIO-93 volume-down assignment does not apply, and no GPIO/keyscan value is
invented here.

Initial ACPI reuses the MT6768 single-core MADT, generic timer, GIC, minimal
DSDT, FADT, and common SSDT. Multicore PSCI/MADT work is deferred until a stable
single-core boot exists.

## Reused MT6768 components

The port reuses `PlatformSecLib`, `GpioImplLib`, `ClockImplLib`,
`PmicWrapperImplLib`, and `MsdcImplLib`, plus the shared GPIO, clock, PMIC
wrapper, MT6358 PMIC, MSDC, eMMC, SD, and simple-framebuffer DXE drivers. None
of those implementations is forked for Barley.

## M2 execution gate

The two-capture memory-map gate is complete. Before controlled execution,
verify framebuffer format/stride, then package the raw AArch64 FD as the kernel
payload of an Android boot header v4 image so LK loads it at `0x40080000`.
Preserve Lenovo preloader, ATF, TEE, LK, GPT, and AVB. Prefer non-persistent
`fastboot boot` after a separately authorized normal unlock; do not flash a
slot merely because a fastboot implementation lacks RAM-boot support.
