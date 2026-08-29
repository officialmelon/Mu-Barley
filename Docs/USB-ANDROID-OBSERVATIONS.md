# TB330XU (Barley) Android USB observations — hardware oracle snapshot

Collected: 2026-08-29, Australia/Brisbane.
Device: Lenovo Tab M11 TB330XU (`barley_row_lte`), serial `HA2452QQ`.
Slot state at collection: switched to slot B (known-good Android) via the
established `fastboot set_active b` + `fastboot reboot` workflow. Slot A remains
the UEFI test slot, untouched.

Android: 15. Kernel: `6.6.77-android15-8-g7731aa7880d9-ab13240798-4k` (GKI).
Build platform: `mt6768`.
Root: **not available** on this slot (`su` not present). All evidence below is
from the unprivileged `shell` user; sections that were root-gated are noted.

Raw outputs referenced here are stored in `Docs/usb-collect/`.

---

## 1. Reaching Android (safe procedure)

```
fastboot getvar product          -> barley_row_lte
fastboot getvar current-slot     -> a
fastboot getvar unlocked         -> yes
fastboot getvar slot-successful:a -> no     (UEFI test slot)
fastboot getvar slot-successful:b -> yes    (Android recovery slot)
fastboot getvar slot-unbootable:a -> no
fastboot getvar slot-unbootable:b -> no
fastboot set_active b && fastboot reboot
```

No flashing, no erase, no partition changes. Slot metadata change only.

## 2. Android USB properties

`getprop` (see `usb-collect/getprop-usb.txt`):

| Property | Value |
|---|---|
| `sys.usb.controller` | `musb-hdrc` |
| `sys.usb.config` | `adb` |
| `sys.usb.state` | `adb` |
| `sys.usb.configfs` | `1` |
| `persist.sys.usb.config` | `adb` |

`sys.usb.controller = musb-hdrc` is init's UDC name — the Android userspace
itself drives the MUSB device controller through `/sys/class/udc/musb-hdrc`.

## 3. UDC / device controller

```
ls /sys/class/udc/            -> musb-hdrc
cat /sys/class/udc/*/uevent   -> USB_UDC_NAME=musb-hdrc
                                 USB_UDC_DRIVER=g1
cat /sys/class/udc/*/state    -> configured
cat /sys/class/udc/*/function -> g1
```

- The UDC name is `musb-hdrc`: **the kernel's USB device controller is MUSB**,
  confirmed live.
- The current gadget is `g1` (composite gadget) in state `configured` while
  `sys.usb.configfs=1` and `sys.usb.state=adb`; init composes functions through
  configfs (kernel config `CONFIG_USB_CONFIGFS=y`).

## 4. Role switch

```
ls /sys/class/usb_role/                    -> mt_usb-role-switch
cat /sys/class/usb_role/mt_usb-role-switch/role  -> device
```

Symlink parent: `/sys/devices/platform/mt_usb/usb_role/mt_usb-role-switch`.

- The role switch is registered by the **`mt_usb` platform device** (the MUSB
  glue), not by the TCPC or the PHY driver.
- Current role: `device` (connected to PC over the data cable).

## 5. Type-C / TCPC

No standard `/sys/class/typec` port (vendor framework instead):

```
ls /sys/class/tcpc/            -> type_c_port0
readlink /sys/class/tcpc/type_c_port0/device  -> ../../../6-004e
readlink /sys/class/tcpc/type_c_port0/of_node -> .../firmware/devicetree/base/
    i2c@1100d000/rt1711_type_c_port0@4e
readlink /sys/bus/i2c/devices/6-004e/driver   -> rt1711h
```

- **TCPC: Richtek RT1711H**, I2C address `0x4E` on the I2C bus whose
  controller is `0x1100D000` (bus 6), DT node
  `i2c@1100d000/rt1711_type_c_port0@4e`.
- TCPC driver: vendor `tcpc_rt1711h` under MediaTek `tcpc_class` (module dir
  `/sys/module/tcpc_class`), **not** the upstream TCPM/`typec_rt1711h` (kernel
  config has `CONFIG_TYPEC_TCPM=y` but `CONFIG_TYPEC_RT1711H is not set`).
- TCPC interrupt: `/proc/interrupts` line `mtk-eint 41 Level  type_c_port0-IRQ`
  (EINT pin 41, level-triggered). It had accumulated 7 events since boot.

The port0 attribute files (`cc_high`, `remote_state`, `vbus_level`,
`typec_role`, `pe_ready`, ...) are root-only; only the directory/link
structure above is shell-readable.

## 6. extcon

`/sys/class/extcon` is root-only (directory listing denied, files denied).
Shadow evidence:

- Built-in module present: `extcon_mtk_usb` (`/sys/module/extcon_mtk_usb`) —
  MediaTek's USB extcon provider used by the MUSB glue for ID/VBUS events.
- The charger framework and TCPC feed it; no read access on this slot.

## 7. Power supply / VBUS / charger state (state B: attached to PC)

`/sys/class/power_supply` (all shell-readable, see `usb-collect/powersupply.txt`):

| Supply | type | online | present | voltage_now | current_now | usb_type | status |
|---|---|---|---|---|---|---|---|
| battery | Battery | — | 1 | 4131000 µV | +477000 µA | — | Charging |
| charger | USB | 1 | — | 5043 µV | — | `Unknown [SDP] DCP CDP ACA BrickID PD` | Charging |
| mtk-master-charger | Unknown | 1 | 1 | 5043 µV | 1 | `Unknown SDP DCP CDP PD PD_PPS` | — |
| mtk-mst-div-chg | Unknown | 0 | 0 | 5043 µV | 1 | — | — |
| mtk-mst-hvdiv-chg | Unknown | 0 | 0 | 5043 µV | 1 | — | — |
| mtk-slave-charger | Unknown | 1 | — | 5043 µV | 1 | — | — |

- `charger.usb_type = [SDP]` — the PC is detected as a Standard Downstream Port
  (data + 500 mA): `current_max = 500000 µA`, `voltage_max = 5000000 µV`.
- Charger architecture is MediaTek's multi-charger framework: a master charger
  (online) plus slave/divider charger instances. The master charger driver
  chain is `mtk_charger_framework` (module link on `/sys/bus/platform/drivers/charger`).
- The `mt6370*` modules exist (`mt6370_charger`, `mt6370_adc`, `mt6370_regulator`,
  `leds_mt6370_flash`) but **no I2C client binds the `mt6370` driver** on this
  board — the charger master is **not** MT6370.
- OTG VBUS sourcing path: platform device `usb-boost-manager` bound to driver
  `mediatek,usb-boost` (module `musb_boost`), plus DT node
  `/usb-boost-manager` (compatible `mediatek,usb-boost-manager` with
  interconnect/OPP references). This is a discrete 5 V boost manager for
  host-mode VBUS — **not** a PMIC regulator read from a charger class.
- Charger status interrupt: `mtk-eint 20 Edge chr_stat` (EINT 20).

## 8. Interrupts (see `usb-collect/interrupts.txt`)

USB-relevant lines from `/proc/interrupts` (8 CPUs):

```
 69:  7  0  0  0  0  0  0  0  mtk-eint  41 Level  type_c_port0-IRQ
 48:  3  0  0  0  0  0  0  0  mtk-eint  20 Edge   chr_stat
232: 1140 0 0 0 0 0 0 0  GICv3 129 Level  musb-hdrc
```

- **`GICv3 129 Level musb-hdrc`** — the MUSB controller interrupt is hardware
  IRQ 129 on the GIC (SPI 97 + 32 = GSI 129), **level-triggered**, and it is
  actively firing (1140 events) — the MUSB interrupt line and GSI mapping are
  confirmed live, not inferred.
- `type_c_port0-IRQ` on EINT 41: the RT1711H CC/interrupt line.
- `chr_stat` on EINT 20: charger status line.
- Touch (EINT 1 `himax_tp`), microSD card-detect (EINT 18), PMIC (EINT 144)
  also visible, consistent with the existing bring-up notes.

## 9. Platform devices (see `usb-collect/plat-dev-11.txt`)

USB-related platform devices:

```
11200000.usb0        <- MUSB controller (driver mt_usb -> creates musb-hdrc)
11210000.usb1p_sif   <- USB1 PHY SIF region
11cc0000.usb-phy     <- T-PHY (driver mtk-tphy)
usb-boost-manager    <- OTG VBUS boost manager (driver mediatek,usb-boost)
charger              <- mtk_charger_framework master consumer
```

Host-side bus while in device role:

```
/sys/devices/platform/mt_usb/musb-hdrc/usb1  ("MUSB HDRC host driver",
  USB 2.00, speed 480, bMaxPower 0mA — root hub only, no attached devices)
```

MUSB registers both host (HCD) and gadget sides unconditionally; role decides
which one operates.

## 10. Driver inventory (see `usb-collect/drv.txt`)

`/sys/bus/platform/drivers/` USB-relevant:

```
mt_usb          (binds 11200000.usb0; module musb_main)
musb-hdrc       (module musb_hdrc; the MUSB core as UDC/HCD)
mtk-tphy        (binds 11cc0000.usb-phy)
mtk-extcon-usb  (USB extcon provider)
usb_phy_generic
mtk-usb-meta    (usb_meta feature)
mediatek,usb-boost (binds usb-boost-manager)
charger         (mtk_charger_framework)
mediatek,usb-boost
```

`/sys/module/` also shows built-in `musb_hdrc`, `musb_main`, `phy_mtk_tphy`,
`tcpc_class`, `tcpc_cps8851` (present, not bound), `extcon_mtk_usb`,
`usbcore`, `usbhid`, `usb_storage`, `usbmon`, `usb_f_uvc`, `mtk_usb_f_rndis`,
`hq_usbnet` (vendor NIC gadget, out-of-tree).

## 11. Kernel config (see `usb-collect/config`)

Pulled via `adb pull /proc/config.gz`. USB-relevant values:

```
CONFIG_USB=y                 CONFIG_USB_OTG=y
CONFIG_USB_MUSB_HDRC is NOT SET      <- MUSB is a vendor module (musb_main)
CONFIG_USB_GADGET=y          CONFIG_USB_CONFIGFS=y
CONFIG_USB_F_MASS_STORAGE=y  CONFIG_USB_CONFIGFS_MASS_STORAGE=y
CONFIG_USB_F_FS=y            CONFIG_USB_CONFIGFS_F_HID=y
CONFIG_TYPEC=y   CONFIG_TYPEC_TCPM=y   CONFIG_TYPEC_TCPCI=y
CONFIG_TYPEC_RT1711H is NOT SET      <- RT1711H runs under vendor tcpc_class
CONFIG_MFD_MT6370 is NOT SET         <- no MT6370 PMIC on this board
CONFIG_PHY_MTK_TPHY is not in GKI defconfig list (vendor module phy_mtk_tphy)
```

Conclusion: **the entire USB controller/PHY/TCPC stack is vendor code delivered
as modules** (`musb_main`, `musb_hdrc`, `phy_mtk_tphy`, `tcpc_class`,
`tcpc_rt1711h`, `extcon_mtk_usb`, `musb_boost`); the GKI kernel only carries
class drivers (HID, storage, configfs functions).

`musb_hdrc` module parameters (`/sys/module/musb_hdrc/parameters/`,
see `usb-collect/musb-params.txt`) reveal the vendor MUSB feature set:

```
iddig_cnt, sw_deboun_time                    (ID pin debounce)
musb_host_dynamic_fifo, musb_host_dynamic_fifo_usage_msk  (dynamic FIFO)
isoc_ep_end_idx, isoc_ep_gpd_count           (QMU isochronous support)
mtk_host_qmu_concurrent, mtk_host_qmu_pipe_msk, mtk_qmu_max_gpd_num,
mtk_qmu_dbg_level                            (QMU DMA)
typec_control, vbus_control, vbus_on         (role/VBUS control hooks)
host_plug_in_test_period_ms, host_plug_out_test_period_ms,
host_test_vbus_only, host_test_vbus_off_time_us  (host test hooks)
musb_force_on, musb_fake_CDP, option, debug, ...
```

This confirms the vendor MUSB has **MediaTek QMU DMA** (`musb_qmu`) and
software role/VBUS control hooks — the same code family as the public alps
sources we collected under `C:\Users\braxt\TB330XU\vendor-src\`.

## 12. Kernel log access

`dmesg` and `/dev/kmsg` are restricted (no root). `logcat -b kernel` is empty.
Kernel-log deltas for role tests therefore need a rooted environment or a
serial/UEFI capture path; sysfs state polling (sections 3–7) works without root.

## 13. Devicetree

- `/sys/firmware/devicetree/base` directory structure is shell-readable, but
  **property files are root-only** (0400) and `/sys/firmware/fdt` is
  permission-denied, so values could not be dumped on-device.
- Node inventory (names + links, from directory reads):
  - `usb0@11200000` with properties `compatible, reg, interrupts,
    interrupt-names, dr_mode, mode, multipoint, num_eps, clocks, clock-names,
    phys, port, usb-role-switch, usb_phy_offset, infracg, pericfg,
    wakeup-source` and child `port/endpoint@0`.
  - `usb-phy@11cc0000` with child `usb-phy@11cc0800` carrying
    `mediatek,discth / eye-vrt / eye-term / rev6 (+ -host variants)` tuning,
    nvmem-cell properties (efuse-based PHY calibration) and a `clock-names`
    entry.
  - `usb1p_sif@11210000`, `usb_meta` (`udc` link), `usb-boost-manager`.
- Cross-reference used instead: the in-repo LK DT for the same SoC family
  (`Mu-Silicium/Resources/DTBs/lancelot.dts`, MT6768) and the U-Boot MT6768
  tree (`Tools/mt6768-mainline-u-boot/arch/arm/dts/mt6768.dtsi`):
  - `usb0@11200000 { compatible = "mediatek,mt6768-usb20";
    reg = <0 0x11200000 0 0x10000 0 0x11cc0000 0 0x10000>;
    interrupts = <0 0x61 8>; mode = <2>; multipoint = <1>; num_eps = <0x10>;
    clocks: "usb0", "usb0_clk_top_sel", "usb0_clk_univpll3_d4" }`
  - `usb2phy: t-phy@11cc0000 { compatible = "mediatek,generic-tphy-v1";
    reg = <0 0x11cc0000 0 0x800> }` with
    `usb2port0: usb-phy@11cc0800 { reg = <0 0x11cc0800 0 0x100>; tuning:
    mediatek,discth = <0xf>; eye-vrt = <0x7>; eye-term = <0x3>; rev6 = <0x3>;
    disc-host = <0x9>; eye-vrt-host = <0x7>; eye-term-host = <0x3>;
    rev6-host = <0x3> }`

## 14. Vendor kernel source status

Lenovo has **not released** the TB330XU kernel sources (open request
https://github.com/lenovo/gplcc/issues/13). The register-level oracle for the
vendor driver family is therefore:

1. `C:\Users\braxt\TB330XU\Tools\mt6768-mainline-u-boot\drivers\usb\musb-new\mt6768.c`
   — MT6768 MUSB glue that boots fastboot-over-USB on this exact SoC.
2. Public alps vendor kernel (MT6761/MT6765, same MUSB/PHY IP generation):
   `https://github.com/deadman96385/android_kernel_zte_mt6761`
   — `drivers/misc/mediatek/usb20/` (`usb20.c`, `musb_core.c`, `musb_host.c`,
   `musb_gadget.c`, `musb_gadget_ep0.c`, `mtk_musb_reg.h`, `mtk_qmu.c/h`,
   `usb20_otg_if.c`, `usb20_phy.c`, `mtk-phy-a60810.h`), collected locally in
   `C:\Users\braxt\TB330XU\vendor-src\`.
3. Upstream Linux `drivers/phy/mediatek/phy-mtk-tphy.c`
   (generic-tphy-v1 support) and `drivers/usb/musb/mediatek.c`,
   also in `vendor-src\phy\`.

The live module-parameter names (`mtk_qmu_*`, `isoc_ep_gpd_count`,
`musb_host_dynamic_fifo`) match the alps vendor MUSB feature set, confirming
the family match.

## 15. Current state at capture (state B)

- Tablet connected to PC by USB-C data cable, role `device`, UDC `g1`
  configured, adb functional, charger sees SDP at 5.0043 V / 500 mA.
- MUSB root hub registered but empty (no host-mode devices).
- No role switching was performed; no sysfs writes were made.

---

# UPDATE 2 — root-confirmed capture (same day, after `adb root`)

`adb root` works on this build (adbd restarts as uid 0). With root the
previously gated evidence is all readable; the additions below supersede
guesses in the first capture where they differ.

## 16. The unplug/replug sequence, verbatim from the kernel ring

Unplug (t≈9402.68 s):

```
TCPC-TCPC:Alert:0x0001              <- CC change interrupt from the TCPC
TCPC-TYPEC:[CC_Alert] 0/0           <- both CC open (cable out)
[MUSB]musb_stage0_irq: SUSPEND (b_peripheral) devctl 99
TCPC-TYPEC: Attached-> NULL / usb_port_detached
TCPC-PE: PD-> PE_IDLE1/PE_IDLE2
mtk-extcon-usb extcon_usb: old_state=1, new_state=0 "Type-C plug out"
mtk-extcon-usb extcon_usb: cur_dr(2) new_dr(0)
musb-hdrc musb-hdrc: mailbox VBUS_OFF
[MUSB]mt_usb_disconnect: USB disconnect -> issue_connection_work ops<0>
[MUSB]nuke ... ep1out               (requests retired)
mt_usb: mt_usb_role_sx_set: if vbus_event false
[MUSB]musb_stage0_irq: DISCONNECT (b_peripheral) as Peripheral, devctl 80
QMU_WARN musb_disable_q_all
mtk-extcon-usb: source vbus = 0mv
[sc8989x_primary_chg] disable charging
```

Replug (t≈9416.66 s):

```
mtk_ctd: handle_typec_pd_attach port0 attach = 0 --> 2   (2 = SDP)
mtk-extcon-usb extcon_usb: old_state=0, new_state=1 "Type-C SINK plug in"
mtk-extcon-usb extcon_usb: cur_dr(0) new_dr(2)
mt_usb: mt_usb_role_sx_set: if vbus_event true
[MUSB]set_usb_phy_clear: Clear PHY setting, 0x6c=0       <- DTM1 zeroed
musb-hdrc: mailbox VBUS_VALID
[MUSB]mt_usb_connect: USB connect ops<2>
[MUSB]do_connection_work: is_host<0>, power<0>, ops<2>
[MUSB]musb_start: start, is_host=0 is_active=0
[MUSB]mt_usb_enable: begin <0,0>,<3,2,2,2> -> end <3,2,3,2>
[MUSB]spm_resource_req_usb: USB_DPIDLE_FORBIDDEN
[MUSB]musb_start: set ignore babble MUSB_ULPI_REG_DATA=89
[MUSB]musb_start: add softconn                           <- POWER.SOFTCONN
[MUSB]set_usb_phy_mode 110: force PHY to mode 6, 0x6c=3f2f   <- DTM1 device recipe
(+180 ms)
[MUSB]musb_stage0_irq: MUSB_INTR_RESET (b_idle)          <- host bus reset
QMU_WARN musb_disable_q_all
[MUSB]musb_stage0_irq: SUSPEND (b_peripheral) devctl 99
```

A later gadget restart (init re-running init.mt6768.usb.rc) shows the
function-side endpoint bring-up verbatim:

```
[MUSB]fifo_setup: musb type=BULK, EP1 supports DBBUF
[MUSB]fifo_setup: fifo size is 22 after 512, fifo address is 512,  epnum 1,hwepnum 1
musb-hdrc periph: enabled ep1out for bulk OUT, maxpacket 512
[MUSB]fifo_setup: fifo size is 22 after 512, fifo address is 1536, epnum 1,hwepnum 1
musb-hdrc periph: enabled ep1in  for bulk IN,  maxpacket 512
```

Facts extracted:

- DEVCTL observed live: **0x99** (BDEVICE + VBUS ≥ VBUSVALID + HR) while
  attached; **0x80** (BDEVICE only) after VBUS_OFF.
- The device-mode PHY register recipe is `DTM1(0x6C) = 0x3F2F`
  (`MTK_DTM1_DEVICE_SET` in MtkUsbPhyRegs.h), written after a full
  `0x6C = 0` clear, followed ~180 ms later by the host bus reset.
- The adb function uses EP1 IN + EP1 OUT bulk, 512 B, **DBBUF (double
  packet buffering)**, FIFOSZ value 0x22, FIFO addresses 512 (OUT) and
  1536 (IN).
- QMU is active in the vendor driver (disable_q_all on every reset).
- Role event plumbing: TCPC alert → `mtk-extcon-usb` (state + data role
  change) → `mailbox VBUS_VALID/OFF` into musb-hdrc → `mt_usb_connect/
  disconnect` work items → musb_start/stop with SPM dpidle forbidding.

## 17. Live devicetree (decompiled from /sys/firmware/fdt)

`barley-live.dtb` is in this directory. USB-relevant values:

- `usb0@11200000`: compatible `mediatek,mt6768-usb20`;
  reg = `0x11200000 0x10000` + `0x11cc0000 0x10000` (**64 KiB windows**);
  interrupts = `<0 0x61 4>` (SPI 97, flags 4 = level);
  mode=2, multipoint=1, num_eps=0x10; dr_mode="otg"; usb-role-switch;
  wakeup-source; cdp-block; `usb_phy_offset = 0x800`;
  clock-names **sys_clk / ref_clk / src_clk** =
  infracfg_ao clock #8 / topckgen #0x66 / topckgen #0x20
  (= INFRA_ICUSB / TOP_USB_TOP_SEL / TOP_UNIVPLL3_D4);
  pericfg phandle present (the "usb0" pericfg gate); phys →
  usb-phy@11cc0800 PHY_TYPE_USB2.
- `usb-phy@11cc0000`: `mediatek,generic-tphy-v1`, reg `0x11cc0000 0x800`.
- `usb-phy@11cc0800`: reg `0x11cc0800 0x100`; **Barley tuning (differs
  from lancelot): mediatek,discth=7, disc-host=0xB, eye-vrt=7 (both),
  eye-term=4, eye-term-host=6, rev6=2 (both)**; nvmem cell `intr_cal`
  (mask 0x1F) from efuse; clock-names "ref" (clk26m phandle).
- `usb1p_sif@11210000`: `mediatek,usb1p_sif`, reg `0x11210000 0x10000`.
- `usb-boost-manager`: compatible **"mediatek,usb-boost",
  "mediatek,mt6768-usb-boost"**; interconnects/required-opps (DVFS-managed
  5 V boost); `usb-audio`; `small-core` handle.
- `usb_meta`: compatible `mediatek,usb-meta`, udc → usb0.
- `extcon_usb`: compatible `mediatek,extcon-usb`, tcpc="type_c_port0",
  `mediatek,bypss-typec-sink`, `mediatek,u2`; its of-graph endpoint is
  linked to `usb0@11200000/port/endpoint@0`.
- `tcpc_pd_eint`: EINT pin 41 (0x29) interrupt spec.

## 18. Corrections to the Type-C and charger story

- **TCPC chip**: the node `i2c@1100d000/rt1711_type_c_port0@4e` has
  compatible **`cps,cps8851`** and PD ID registers VID 0x315C PID 0x8851
  ("CPS_TCPC") — the silicon is a **CPS8851**, not an RT1711H; the node
  name and the bound driver name (`/sys/bus/i2c/devices/6-004e/driver ->
  rt1711h`) are vendor leftovers. Both `tcpc_cps8851` and `rt1711h` vendor
  drivers exist in the module list; the DT compatible is the authority.
  TCPC attributes as root: `remote_state=07`, `vbus_level=2`,
  `typec_role=TrySNK`, `local_rp_level=Default`, `pe_ready=no`.
- **Charger**: the actual charge IC is an **SC8989X at I2C bus 7 address
  0x6A** (`/sys/bus/i2c/devices/7-006a -> sc8989x`, "primary_chg" in the
  charger framework). Fuel gauge: **mm8013 at 7-0055**. The DT contains
  an `mt6370@34` charger+TCPC block on I2C5 (0x11016000) with an
  `usb-otg-vbus-regulator`, but its client is **not bound** — the MT6370
  is a dead node on barley. `slave_charger@4b` likewise unbound.
- extcon (root): exactly one provider, `extcon0 = extcon_usb`, state
  `USB=1 USB-HOST=0` while attached to the PC.

## 19. Live clocks (debugfs clk_summary)

```
ifr_icusb        1 1 0  156000000   -> mt_usb sys_clk
usb_top_sel      1 1 0   62400000   -> mt_usb ref_clk   (parent univpll3_d4)
univpll3_d4      2 2 0   62400000   -> mt_usb src_clk
usb20_192m_ck    0 0 0   192000000  (unused on this board)
phy-...usb-phy.0 ref                  (26 MHz reference)
```

## 20. MUSB module parameters of interest (now with values)

```
typec_control=1, vbus_control=1, vbus_on=N
iddig_cnt=0, sw_deboun_time=400
host_test_vbus_only=1 (test hook default)
musb_host_dynamic_fifo (dynamic FIFO in host mode too)
mtk_qmu_max_gpd_num / isoc_ep_gpd_count (QMU rings)
```

## 21. UPDATE 3 — test C: OTG adapter + keyboard (host role), via wireless adb

Wireless adb (`adb tcpip 5555` + `connect 192.168.1.115:5555`, root) stayed
alive throughout the test with the USB-C port occupied by the OTG adapter.
Pre-state: role `device`, UDC `configured`, extcon `USB=1/USB-HOST=0`.

Host-role transition, verbatim (t≈10873-10878 s):

```
mt_usb: role_sx_set role 0, latest_role: 2          <- PC cable removed
mt_usb: mt_usb_role_sx_set: if vbus_event false
sc8989x: input current limit -> 100 mA, charger disabled
mt_usb: role_sx_set role 1, latest_role: 0          <- OTG adapter attached
mt_usb: mt_usb_role_sx_set: if id_event true        <- ID event = role trigger
[MUSB]mt_usb_host_connect: connect
[MUSB]issue_host_work ops<2>, on_st<1>
[MUSB]do_host_work: PASS, init_done:1, is_ready:1, inited:0
[MUSB]musb_start: start, is_host=1 is_active=0
[MUSB]musb_stage0_irq: CONNECT (a_host) devctl 5d
usb 1-1: new full-speed USB device number 2 using musb-hdrc
usb 1-1: idVendor=0c45, idProduct=8006 "SONiX USB DEVICE"
hid-generic: USB HID v1.11 Keyboard ... input0
hid-generic: ... Consumer Control / System Control / Mouse (interface 1)
musb_hub_control: port status 00000101 -> 00120103, devctl=0x5d
musb_port_reset: force musb_platform_reset
musb_h_ep0_irq: STALLING ENDPOINT (x3, benign EP0 stalls during enumeration)
```

Post-state: role `host`, UDC `not attached`, extcon `USB=0 / USB-HOST=1`,
bus: `usb1` + `1-1` with three HID interfaces.

Facts extracted:

- **Role trigger in host direction is `id_event true`** — the TCPC/extcon
  chain reports ID/attach; the MUSB glue then runs `mt_usb_host_connect` →
  `issue_host_work` → `musb_start(is_host=1)`.
- **DEVCTL in host mode = 0x5D**: `HM`(0x04) | `SESSION`(0x01) |
  `FSDEV`(0x40, full-speed device attached) | VBUS field = 0b11 (VBUS above
  VBUSVALID — the tablet is SOURCING 5 V).
- **VBUS sourcing is silent** — no "boost" log lines. Power flow proves it:
  battery `Discharging` at ~302 mA, `charger.voltage_now = 5080` (the 5 V
  rail), charger `online=0`. The keyboard enumerated and drew power.
- **VBUS boost is a regulator**: `usb-boost-manager` sysfs lists
  `supplier:regulator:regulator.47` and
  `supplier:platform:10012000.dvfsrc` — the boost rail is managed through
  the regulator framework (DVFS Resource Controller involved). The exact
  enable primitive lives in the `musb_boost` built-in (no .ko on disk);
  decoding it needs the kernel image or a live trace.
- The vendor PD engine (in TCPC) tries `PE_SRC_SEND_CAPABILITIES` and gets
  `tx_err` loops against the non-PD keyboard — harmless, and UEFI will not
  run a PD stack at all.
- Host enumeration of a real device (full-speed composite HID) works
  end-to-end in Android: root-hub port status changes, port reset, EP0
  handshake with stalls, descriptor fetch, HID probe — all via
  `musb-hdrc`. This is exactly the path `MtkMusbHostDxe` replicates.

## 22. UPDATE 4 — test D: flash drive (host role, MSC/BOT)

Drive swapped onto the OTG adapter (role stayed `host`, VBUS boost stayed
on across the swap). Wireless adb dropped once mid-test (WiFi power save)
and recovered with `adb connect` — no USB involvement.

```
[keyboard removed]
musb_hub_control: port status 00000104, devctl=0x19 -> 0x59   (connect-change idle)
[drive attached]
musb_hub_control: port status 00010105 -> 00000105 -> 00120507, devctl=0x5d
usb 1-1: new high-speed USB device number 3 using musb-hdrc
usb 1-1: idVendor=0781, idProduct=5567, Product: Cruzer Blade
usb-storage 1-1:1.0: USB Mass Storage device detected
scsi host0: usb-storage 1-1:1.0
scsi 0:0:0:0: Direct-Access  SanDisk Cruzer Blade 1.00 PQ: 0 ANSI: 6
sd 0:0:0:0: [sda] 60125184 512-byte logical blocks (30.8 GB)
sda: sda1  (FAT)
```

Facts:

- The drive enumerated at **high speed (480 Mbps)** — both the MUSB host
  high-speed path and the PHY tuning are proven at HS, not just FS.
- MSC Bulk-Only Transport works: INQUIRY / READ CAPACITY / partition scan
  all flowed over EP1 bulk through the same PIO/QMU paths adb uses.
- The boost rail is **sticky per role**: it stayed enabled through the
  device swap; only the role change (or unplug) gates it.
- Host-mode test matrix complete: HID keyboard (FS) ✓, MSC flash drive
  (HS) ✓, VBUS sourcing ✓, role enter/exit ✓.
