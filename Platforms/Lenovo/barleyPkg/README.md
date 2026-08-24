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

## Live-FDT memory map

Two independently captured LK-patched FDTs establish physical RAM as
`0x40000000 + 0x200000000`. Their `/memory/reg`, all 23 fixed
`/reserved-memory/*/reg` entries, and complete 22,552-byte MediaTek
`/memory/mblock_info` properties are byte-identical:

| Capture | SHA-256 |
| --- | --- |
| `live-barley.dtb` | `A09ED364C2E98E3E08CBB56D9AD7EC486007486833A0BA25E05E616BE607CE4A` |
| `live-barley-2.dtb` | `4DD7BB763DE6EED898E2D085DEF7D29308FF37D6F8B48C6DCF5DE6C039E40070` |

Lenovo's 64-bit LK jump ABI passes the patched DTB as the first argument. The
BootShim preserves `x0`, and Barley's earliest SEC hook saves it in the reserved
handoff page at `0x40040000`. `MemoryMapLib` prefers that pointer and retains
`0x4BC80000` only as a compatibility fallback.

The live map is constructed from `/memory/reg` and MediaTek's native
`/memory/mblock_info` free list. Every `/reserved-memory` child `reg`, the live
DTB itself, UEFI stack/FD/heap, and both distinct display reservations are
subtracted before conventional-memory HOBs are published. Fixed `no-map`
reservations remain unmapped; the one explicit exception is LK's framebuffer
carveout, which the passive GOP must access. No node count, boot mode, or
volatile chosen property is used as a validity gate.

The former provisional `0x40280000 + 0x03C00000` heap is gone. SEC/DXE uses
stable mblock 7 at `0x56000000 + 0x16000000`. If no dynamic LK map can be
parsed, the implementation falls back only to the mblocks that were
byte-identical in both physical captures. It never flattens the physical 8 GiB
range, and `RamManagerDxe` remains excluded for the same reason.

`BarleyLkGopDxe` wraps the display pipeline already configured by Lenovo LK; it
does not initialize DSI, reset the panel, change clocks/timings, or access
display MMIO. LK expdb proves an OVL direct-link handoff with `eBGRA8888`,
1200 x 1920 layers. Layer 0 alternates between `0x7BCE0000` and `0x7CEB0000`
at a 4864-byte pitch, while the full-screen layer-3 surface at `0x7C5C8000`
uses a 4800-byte pitch. All three live inside the verified framebuffer
carveout and the aligned allocations are spaced by `0x008E8000`.

Register readback after LK's handoff does not identify the composited buffer
pool reliably enough to gate GOP installation. The passive GOP therefore
mirrors firmware BLTs to all three verified surfaces with each surface's own
stride and forces the BGRA alpha byte to `0xFF` for fills and buffer-to-video
writes. SEC preserves LK's layer configuration and only enables the MT6768
constant-blend bit on the stable full-screen layer 3, matching the shared
MT6768 behavior for an OS-owned BGRX framebuffer. The GOP publishes that layer
as the standard linear mode used after `ExitBootServices()`: BGRR8888 at `0x7C5C8000`,
1200 x 1920, 1200 pixels per scan line, and a `0x008CA000`-byte framebuffer.
The distinct LK logo decompression allocation at `0x7A3F8000` and FDT display
reservation at `0x7E605000` are not treated as scanout or exposed as
conventional memory. Only the verified OVL framebuffer carveout is mapped for
GOP access. LK free mblock 11 remains unavailable because it intersects the
separate display reservation.

Buttons are intentionally omitted. Android evidence shows power and
volume-down on `mtk-pmic-keys`, while volume-up is on `mtk-kpd`. Lancelot's
GPIO-93 volume-down assignment does not apply, and no GPIO/keyscan value is
invented here.

Initial ACPI reuses the MT6768 single-core MADT, generic timer, GIC, minimal
DSDT, FADT, and common SSDT. Multicore PSCI/MADT work is deferred until a stable
single-core boot exists.

## Reused MT6768 components

The port reuses `GpioImplLib`, `ClockImplLib`, `PmicWrapperImplLib`, and
`MsdcImplLib`. Barley overrides only `PlatformSecLib`: Lenovo LK has already
entered at EL1, so its assembly initializer only preserves the incoming FDT
pointer. Its C initializer disables TOPRGU and applies the shared MT6768
constant-blend operation to LK's actual full-screen layer 3. Standard DXE
components provide console, eMMC, microSD, FAT, and the internal shell; Barley
adds hardware-specific code only where the MT6768 implementation needs device
configuration.

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
