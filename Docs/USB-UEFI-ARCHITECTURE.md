# TB330XU USB — UEFI architecture, milestones, and automated test loop

Status: design agreed with the evidence in `USB-HARDWARE-MANIFEST.md`.
Compile-only scaffolding exists in-tree and is verified to build
(`UsbScaffoldTest/` build, CLANGPDB, AARCH64 — see §7). Nothing is wired
into `barley.dsc` / `barley.fdf`.

## 1. Module layout (created)

```
Silicon/MediaTek/MediaTekPkg/
  Include/
    MtkMusbRegs.h                      MUSB core + MediaTek wrapper + QMU globals
    MtkUsbPhyRegs.h                    T-PHY v1 U2 port bank + role bit sequences
    Library/MtkMusbCoreLib.h           core lib API + MTK_MUSB_SNAPSHOT diagnostics
    Library/MtkUsbPhyLib.h             PHY lib API
  Library/
    MtkMusbCoreLib/                    read/write/indexed/FIFO/soft-reset/snapshot
    MtkUsbPhyLib/                      probe/set-mode/power/tuning (no clocks, no VBUS)
  Drivers/
    MtkMusbHostDxe/                    EFI_USB2_HC_PROTOCOL skeleton (nothing installed)
    MtkMusbDeviceDxe/                  gadget skeleton (nothing installed)

Platforms/Lenovo/barleyPkg/Library/BarleyUsbPortLib/
    RT1711H board facts + VBUS-boost guard (refuses to enable VBUS until decoded)
```

Layering rule: `MtkMusbCoreLib` and `MtkUsbPhyLib` are SoC-generic
(MT6768 family), `BarleyUsbPortLib` is board-only. Charger/TCPC/VBUS knowledge
never leaks into the SoC libraries.

## 2. Host path (MtkMusbHostDxe → EFI_USB2_HC_PROTOCOL)

Consumers are stock EDK2 class drivers already present in Mu_Basecore:
`UsbBusDxe`, `UsbKbDxe`, `UsbMouseDxe`, `UsbMouseAbsolutePointerDxe`,
`UsbMassStorageDxe`. We write no class drivers.

Driver responsibilities per bring-up step:

1. **Step H1 — attach safely.** Probe-only DiagnosticDxe variant: map
   0x11200000/0x11CC0000, `MtkMusbSnapshot()` + `MtkUsbPhyPrintStatus()` to
   the on-screen log, compare against the clock-state question (INFRA_ICUSB
   gate bit, TOP_USB_TOP_SEL mux/gate state as left by LK). No writes.
2. **Step H2 — clock + PHY host bring-up.** Enable `INFRA_ICUSB`,
   `TOP_USB_TOP_SEL→UNIVPLL3_D4`, 48 M chain (ClockImplLib IDs already
   defined), `MtkUsbPhySetMode(Host)` + `MtkUsbPhyPowerOn(Host)` +
   tuning; `MtkMusbInitInterrupts()`; `DEVCTL |= SESSION`; read
   INTRUSB connect/disconnect. Milestone: seeing the device-side connect
   event (CDC/CSR0 bits) from the UEFI shell without enumerating.
3. **Step H3 — root hub + EP0 control transfers (PIO).** Implement
   `GetCapability`, `Reset`, `Get/SetState`, `ControlTransfer`,
   `Get/Set/ClearRootHubPort*`. 64-byte PIO FIFO access, 8/16/32-bit
   FIFO reads per packet size. Milestone: `UsbBusDxe` reads device
   descriptors of a hub-less keyboard; shell input works via `UsbKbDxe`.
4. **Step H4 — interrupt + bulk.** `AsyncInterruptTransfer` for HID
   (mouse next), `BulkTransfer` for mass storage. Toggle tracking uses the
   MediaTek RXTOG/TXTOG enable regs already set by `MtkMusbInitInterrupts`.
5. **Step H5 — optional QMU DMA** (vendor GPD rings) once PIO is proven;
   the first revision is deliberately PIO-only.

Isochronous: return `EFI_UNSUPPORTED` (protocol permits; UsbBus/Kb/MSD never
call it).

## 3. Device path (MtkMusbDeviceDxe)

1. **Step D1 — dummy device.** Device role via `MtkUsbPhySetMode(Device)`,
   `MtkMusbSoftReset()`, `DEVCTL |= BDEVICE`, soft-connect
   (`POWER |= SOFTCONN`). Hand-rolled EP0: GET_DESCRIPTOR device/config,
   SET_ADDRESS (FADDR), SET_CONFIGURATION. Milestone: PC enumerates a
   "dummy" device; the PC side is our test oracle (no tablet-side logs needed).
2. **Step D2 — EFI_USBFN_IO_PROTOCOL surface.** Implement the vendor-shaped
   protocol (register/endpoint/transfer primitives) so that the existing
   Silicium `EFI_USB_MSD_PROTOCOL` consumers can ride on it. The Qualcomm
   `EFIUsbfnIo.h` in-tree is the shape reference; the protocol GUID family
   stays Qualcomm-compatible so `UsbMsdDxe`-style consumers can be reused
   later without a rewrite.
3. **Step D3 — MtkMusbMsdDxe.** Bulk-Only Transport over EP1 IN/OUT
   (512 B), SCSI subset INQUIRY / TEST UNIT READY / REQUEST SENSE /
   READ CAPACITY(10) / READ(10). **Read-only first**; writes deferred so
   firmware and host never share a writable block device. LUN assignment
   API mirrors `EFI_USB_MSD_PROTOCOL` (AssignBlkIoHandle/QueryMaxLun/
   StartDevice/EventHandler/StopDevice) so the upstream MassStorage app can
   be ported with a small diff — note the app's Qualcomm vendor-GUID device
   path probe does not match MsdcDxe's MSG_EMMC_DP/MSG_SD_DP paths, so the
   barley MassStorage app must enumerate BlockIo handles directly instead
   of using `StorageVendorDevicePath`.

## 4. Windows phase (planned, later)

- Host: Windows ARM64 ships a generic USB stack; the missing piece is a
  KMDF USB host controller driver (UCX client) for MUSB — essentially the
  same register work as `MtkMusbCoreLib` behind a UCX controller interface,
  plus URS for dual-role policy. Decide only after UEFI host is proven.
- Device: USB Function (UFX) client for MUSB gadget mode, MUSB Mass Storage
  function; or, short-circuit: keep UEFI device mode for provisioning and
  skip UFX initially.
- Role switching: same TCPC/extcon chain; URS/role events map from the
  RT1711H CC state.

## 5. Automated test loop (target design)

Long-term goal: an agent runs one command per iteration and reads a verdict.
Pieces, in dependency order:

1. **Deployment.** `fastboot flash boot_a` (only ever `boot_a`) + `fastboot
   reboot`, slot guard rails asserted first (slot B untouched, getvar checks
   from the handoff doc). Recovery is the documented `set_active b` path.
2. **UEFI-side evidence bus.** A compact, bounded, in-memory diagnostic ring
   (pattern proven by the M2.64 MSDC registry snapshots):
   `MTK_MUSB_SNAPSHOT` + PHY dump + clock states + per-event counters,
   written to a UEFI variable (or pstore region) on every role event.
3. **Screen-visible verdict.** On-screen one-page USB status (bring-up is
   photographable; `X:` logs are not collectable) — reuse the M2.64
   on-screen counter approach.
4. **PC-side oracle for device mode.** PowerShell/`pnputil`/`Get-PnpDevice`
   WMI snapshot of the USB tree on the PC when the tablet enumerates: VID/PID
   presence IS the test result — fully machine-readable, no tablet logs
   needed. This makes device-mode iteration fully closed-loop immediately.
5. **Host-mode oracle.** USB keyboard/mouse/flash drive attached; verdict =
   shell keystroke echo (keyboard), BlockIo presence in a UEFI diagnostic
   app (drive). Automated once a diagnostic DXE prints a machine-parsable
   PASS/FAIL line to the on-screen log.
6. **WinDbg/KD integration (Windows phase).** When the Windows port runs:
   - KD over serial is not available on barley (no exposed UART), so use
     **KDNET over USB** (windbag/KDNET-class) or, realistic near-term,
     **WinDbg over the existing MSDC-free SD/UEFI variable + DbgPrint ring**
     capture on next boot.
   - Failure detection: kd worker script (`!analyze -v`, `.kdfiles`) with a
     watchdog; on timeout → `fastboot reboot` → slot A retry counter.
   - The LLM loop wraps: build → flash boot_a → reboot → wait for PC-side
     enumeration (device mode) or on-screen PASS/FAIL (host mode) → collect
     → decide → iterate. No manual interpretation.

## 6. Physical role-test matrix (Android oracle, from task §6)

Snapshot machinery is ready (`Docs/usb-collect/` script set). Tests:

| Test | Physical action | Data captured before/after |
|---|---|---|
| A | Unplug everything | role, UDC state, extcon (root-gated), charger type |
| B | Connect PC data cable | role→device, g1/configfs state, charger SDP |
| C | OTG adapter + keyboard | role→host?, usb1 bus children, VBUS boost |
| D | OTG adapter + flash drive | usb-storage binding, bus speed |
| E | Powered hub (later) | hub enumeration |

`adb root` works on this build, so every observable (dmesg deltas, extcon,
TCPC attributes, full DTB) is collectable automatically before and after each
physical event - tests A and B are already captured verbatim in
`USB-ANDROID-OBSERVATIONS.md` UPDATE 2. Remaining: C (OTG + keyboard) and
D (OTG + flash drive), which additionally reveal the `usb-boost-manager`
VBUS-enable sequence.

**One action needed from you now** (see final section): nothing is plugged
into the tablet except the PC cable used for adb.

## 7. Scaffold build verification

- `UsbScaffoldTest/UsbScaffold.dsc` + `PlatformBuild.py` — scratch build
  platform, builds the new components against the real dependency graph
  (`!include MT6768Pkg/MT6768Pkg.dsc.inc`).
- Verified: `python UsbScaffoldTest/PlatformBuild.py` → `PROGRESS - Success`;
  `MtkMusbHostDxe.efi`, `MtkMusbDeviceDxe.efi`, both libraries and
  `BarleyUsbPortLib` all compile/link with CLANGPDB AARCH64.
- Known quirk: incremental scratch rebuilds hit an nmake "fatal cycle in
  dependency tree" (stale `.obj.deps` self-references on this host's
  toolchain versions). Clean rebuild works; `build_uefi.py` flows that clean
  first are unaffected. Delete `Build/UsbScaffold` between iterations.
- `barley.dsc`/`barley.fdf` are untouched — no boot behavior changed.
