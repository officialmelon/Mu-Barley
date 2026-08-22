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
| stack `0x40000000..0x4003FFFF` | Existing MT6768/lancelot early layout | PROVISIONAL |
| DXE heap `0x40280000..0x43E7FFFF` | Existing MT6768/lancelot early layout | PROVISIONAL |
| one continuous 8 GiB region from `0x40000000` | No valid source; conflicts with dynamic mblock carveouts | INVALID — NOT USED |

## Conservative M1 constraints

Lenovo preloader/LK dynamically patches the final DRAM and reserved-memory
mblock layout. The static DT is not sufficient to reconstruct all secure,
modem, SCP, SSPM, pstore, framebuffer, and other carveouts. Consequently this
package adds only the early stack, FD, and DXE heap windows as usable memory.
It does not include `RamManagerDxe`, because that shared driver treats detected
MediaTek DRAM as one continuous range and the full Barley reservation list is
not yet available.

The framebuffer is exposed through the existing inherited `SimpleFbDxe` path;
M1 does not initialize DSI or the panel. The physical allocation is verified,
but pixel format, orientation, and pixels-per-scanline must be confirmed before
a display test. The current 1200 x 1920 x 32 configuration is explicitly
provisional.

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

## M2 gate

Before any controlled execution, recover the final runtime FDT/mblock map from
the exact device (for example, after a separately authorized normal bootloader
unlock using the supplied debuggable vendor ramdisk). Reconcile every reserved
range with `MemoryMapLib`, verify framebuffer format/stride, then package the
raw AArch64 FD as the kernel payload of an Android boot header v4 image so LK
loads it at `0x40080000`. Preserve Lenovo preloader, ATF, TEE, LK, GPT, and AVB;
M1 performs no flashing or device mutation.
