# TB330XU (Barley) USB hardware manifest

Every value below is evidence-backed: either observed live on the tablet
(Android 15, GKI 6.6, slot B — see `USB-ANDROID-OBSERVATIONS.md`), extracted
from a source file whose revision matches the live module family, or read
from the in-repo LK/U-Boot MT6768 device trees. Items that still require
live verification before use in firmware are explicitly marked.

## 1. Controller — Mentor MUSB HDRC wrapped by MediaTek ("mt_usb")

| Item | Value | Evidence |
|---|---|---|
| Hardware type | Mentor MUSB HDRC (MTK wrapper, dynamic FIFO, multipoint) | UDC name `musb-hdrc` live; U-Boot glue `mt6768.c` |
| Controller MMIO | `0x11200000` | live platform device `11200000.usb0`; LK DT lancelot; U-Boot dtsi |
| Register window | **`0x10000` (64 KiB)** at `0x11200000` plus **`0x11cc0000 0x10000`** as second reg - live `/proc/iomem` shows `11200000-1120ffff` and `11cc0000-11ccffff` claimed by usb0 | live DTB `reg`; live iomem |
| IRQ (GIC SPI) | 97, DT flags 4 (**level**; live Barley DTB `interrupts = <0 0x61 4>`) | live DTB (lancelot LK DT used flags 8) |
| IRQ (Linux/GSI) | **`GICv3 129 Level musb-hdrc`** — GSI 129 confirmed live, firing | `/proc/interrupts` line 232 |
| Endpoint count | 16 hardware endpoints - live DTB `num_eps = <0x10>`, vendor `MUSB_C_NUM_EPS 16`; U-Boot uses 6 "to avoid issues" | live DTB; vendor `musb_core.h:166` |
| FIFO architecture | Dynamic FIFO, `multipoint`, RAM bits 11 (2 KiB EP RAM per direction in U-Boot cfg); EP0 = 64 B | U-Boot `mt6768.c` fifo config; vendor `musb_core.c` `fifo_setup` |
| Max packet | EP0 64 B; vendor alps/QMU supports bulk up to 512 B (HS); U-Boot configures 512 B single FIFOs | vendor musb_core.c `ep0_cfg`, U-Boot `MUSB_EP_FIFO_SINGLE(...,512)` |
| Host capability | Yes — root hub registered even in device role (`/sys/.../musb-hdrc/usb1`, "MUSB HDRC host driver", USB 2.00/480) | live sysfs |
| Device capability | Yes — Android UDC `musb-hdrc`, gadget `g1`, state `configured` | live sysfs |
| Vendor DMA | MediaTek QMU DMA (GPD ring descriptors, `mtk_qmu.c`) + classic MUSB HSDMA - QMU **confirmed active live** (`QMU_WARN disable_q_all` on every bus reset); adb function uses EP1 IN/OUT bulk 512 B **DBBUF**, FIFOSZ 0x22, fifo addrs 512/1536 | live dmesg; vendor `mtk_qmu.c` |
| Wakeup | `wakeup-source` in DT | live DT node inventory |

## 2. USB2 PHY — MediaTek T-PHY v1 (port 0)

| Item | Value | Evidence |
|---|---|---|
| PHY type | MediaTek T-PHY, `mediatek,generic-tphy-v1` | U-Boot mt6768.dtsi; live driver `mtk-tphy` |
| T-PHY shared base | `0x11CC0000` size `0x800` (IPPC; no U3 in this T-PHY instance) | U-Boot dtsi; live device `11cc0000.usb-phy` |
| USB2 port 0 | `0x11CC0800` size `0x100` — v1 port base == U2 PHY COM bank (`SSUSB_SIFSLV_V1_U2PHY_COM = 0x000`) | U-Boot dtsi; upstream `phy-mtk-tphy.c:30,1081` |
| Reference clock | port clock-names `ref` -> clk26m phandle (26 MHz), live clk_summary shows `phy-11cc0000.usb-phy.0 ref` | live DTB + debugfs clk_summary |
| Tuning (device) | **`discth=7, eye-vrt=7, eye-term=4, rev6=2`** (+ efuse `intr_cal`, mask 0x1F) | live Barley DTB (differs from lancelot/U-Boot dtsi placeholders) |
| Tuning (host) | **`disc-host=0xB, eye-vrt-host=7, eye-term-host=6, rev6-host=2`** | live Barley DTB |
| Calibration | nvmem/efuse cell properties present on the live DT port node (values root-gated) | live DT node inventory |
| Extra SIF | `usb1p_sif@11210000` (USB1 PHY SIF region, second/OTG port per DT name) | live platform device list |

Role-select registers (upstream `phy-mtk-tphy.c`, tphy v1, port base +):

| Register | Offset | Bits | HOST value | DEVICE value |
|---|---|---|---|---|
| `U3P_U2PHYDTM1` | `0x6C` | `P2C_FORCE_IDDIG` BIT(9) | set | set |
| `U3P_U2PHYDTM1` | `0x6C` | `P2C_RG_IDDIG` BIT(1) | clear | set |
| `U3P_U2PHYDTM1` | `0x6C` | `P2C_RG_VBUSVALID` BIT(5) | set | clear |
| `U3P_U2PHYDTM1` | `0x6C` | `P2C_RG_AVALID` BIT(2) | set | clear |
| `U3P_U2PHYDTM1` | `0x6C` | `P2C_RG_SESSEND` BIT(4) | clear | set |

Power-down bits live in `U3P_U2PHYDTM0` (`0x68`: `P2C_FORCE_SUSPENDM` BIT(18),
`P2C_RG_SUSPENDM` BIT(3), data/pulldown/xcvrsel/termsel force bits); the full
power-on/off sequence is in `u2_phy_instance_power_on/off()`. Soft reset via
`U3D_U2PHYDCR0` (`0x60`) `P2C_RG_SIF_U2PLL_FORCE_ON` BIT(24).

## 3. Clocks

| Clock | Role | Register facts | Evidence |
|---|---|---|---|
| `INFRA_ICUSB` (`sys_clk`) | INFRA gate for the USB IC - **live: enabled, 156 MHz** | INFRA_CFG_AO set `0x80`/clear `0x84`/status `0x90`, bit 8; DTB infracfg_ao clock #8 | repo ClockImplLib.c:2482; live clk_summary + DTB |
| `TOP_USB_TOP_SEL` (`ref_clk`) | controller mux+gate - **live: enabled, 62.4 MHz from univpll3_d4** | CKGEN `0x90` (set `0x94`/clear `0x98`/update `0x04`), mux bit 16 w1, gate bit 23; parents `CLK26M` / `UNIVPLL3_D4` | repo ClockImplLib.c:1141; live clk_summary + DTB |
| `TOP_USB20_192M` | 192 MHz USB PLL-derived clock | Factors clock (from UNIVPLL chain) | repo ClockImplLib.c:1519 |
| `TOP_USB20_192M_D4` → `TOP_DA_USB20_48M_DIV` | 48 MHz for USB2 | factor parent `TOP_USB20_192M_D4` ×1/÷1 | repo ClockImplLib.c:2113 |
| `TOP_USB20_48M_EN` | 48 M enable gate | CKGEN set `0x104` | repo ClockImplLib.c:2257 |
| DT clock names | `usb0` (pericfg gate), `usb0_clk_top_sel`, `usb0_clk_univpll3_d4` | lancelot LK DT + vendor `usb20.c:1822-1834` `devm_clk_get("usb0"/"usb0_clk_top_sel"/"usb0_clk_univpll3_d4")` | both |

Whether LK leaves these enabled for UEFI: **unknown — must be probed live**
(the MSDC work established the probe pattern: read gate status registers
before touching them; `PcdMsdcPreserveBootStateMask` is the precedent).

## 4. USB-C / Type-C

| Item | Value | Evidence |
|---|---|---|
| CC controller | **CPS8851** (compatible `cps,cps8851`, PD ID VID 0x315C PID 0x8851 "CPS_TCPC"). Node name `rt1711_type_c_port0@4e` and the bound driver name (`rt1711h`) are vendor leftovers; the DT compatible and ID registers are the authority | live DTB decompile; `/sys/class/tcpc/type_c_port0` links |
| I2C bus/address | bus 6 (controller `0x1100D000`), address `0x4E` | live `/sys/bus/i2c/devices/6-004e` |
| IRQ | EINT 41, level (`type_c_port0-IRQ`), firing on live system | `/proc/interrupts` |
| Framework | MediaTek vendor `tcpc_class` (+ PD stack modules), not upstream TCPM | live `/sys/module/tcpc_class`; kernel config `CONFIG_TYPEC_RT1711H not set` |
| Data role source | TCPC CC detect → vendor tcpc → `mtk-extcon-usb` (state + data-role) → `mailbox VBUS_*` (device) / **`id_event` (host)** → `mt_usb_connect`/`mt_usb_host_connect` → role switch (`mt_usb-role-switch` owned by `platform/mt_usb`) | live dmesg chains (sec.16, sec.21) |
| Host-mode bring-up (Android-observed) | `musb_start(is_host=1)` → root-hub port status `00000101`→`00120103` → `musb_port_reset` → EP0 enumeration (benign STALLs) → HID probe | live dmesg sec.21 |
| Power role source | TCPC (Rp/Rd) + charger framework | charger class + tcpc of_node |
| Orientation | RT1711H reports CC polarity; vendor tcpc exposes it (root-gated attrs) | framework docs in `vendor-src/typec/` |
| Standard typec class | **absent** (`/sys/class/typec` empty) — do not expect upstream UCSI/TCPM behavior | live sysfs |

## 5. VBUS

| Item | Value | Evidence |
|---|---|---|
| Sink detect | CPS8851 TCPC VBUS detect + charger framework (`charger` supply `type=USB`, SDP/DCP/CDP/PD types) | live power_supply |
| OTG source | **`usb-boost-manager`** platform device, driver `mediatek,usb-boost`, module `musb_boost` | live driver binding |
| Source rail | **`usb-boost-manager`**: compatibles `mediatek,usb-boost` + `mediatek,mt6768-usb-boost`; exposes `supplier:regulator:regulator.47` + `supplier:platform:10012000.dvfsrc` - the 5 V boost is a **regulator-framework rail**; enable primitive inside built-in `musb_boost`, not yet decoded; **DO NOT drive it from UEFI until decoded** | live DTB + host-mode sysfs (test C) |
| Source verify (test C) | Host mode: battery Discharging ~302 mA feeding boost, `charger.voltage_now = 5080`, keyboard enumerated and powered; DEVCTL 0x5D (VBUS=0b11) | live dmesg + power_supply, observations sec.21 |
| PMIC path | `mt6370@34` DT block (charger + tcpc + `usb-otg-vbus-regulator` 4.26-5.88 V / 3 A) exists on I2C5 0x34 but its client is **unbound** - dead node on barley | live i2c driver bindings + DTB |

## 6. Charger

| Item | Value | Evidence |
|---|---|---|
| Framework | `mtk_charger_framework` (`charger` platform device, master consumer) | live driver/module link |
| Charge IC | **SC8989X at I2C bus 7 address 0x6A** (driver `sc8989x`, framework name "primary_chg") - disable/enable charging confirmed in dmesg on unplug/plug | live `/sys/bus/i2c/devices/7-006a`; live dmesg |
| Fuel gauge | mm8013 at I2C bus 7 address 0x55 (`fg_mm801310c` logs) | live driver binding |
| Charger instances | `mtk-master-charger` (online), `mtk-slave-charger`, `mtk-mst-div-chg`, `mtk-mst-hvdiv-chg` (divider charge pumps) | live power_supply |
| Status IRQ | EINT 20 `chr_stat` | `/proc/interrupts` |
| Relationship to USB | BC1.2/PD detection reported through charger class (`usb_type`); TCPC feeds PD; the MUSB glue consumes charger-type via `mtk_boot_common`/`charger_type` APIs in alps source | vendor `usb20.c` includes |

## 7. Android driver chain (actual, live-verified)

```
USB-C connector
   ↓  CC/VBUS/Rp-Rd
CPS8851 TCPC @ I2C6 0x4E         (IRQ: EINT 41, "type_c_port0-IRQ")
   (bound by vendor "rt1711h" driver name; compatible cps,cps8851)
   ↓  tcpc_class events
MediaTek tcpc framework / PD stack (vendor modules)
   ↓  extcon + role events
extcon_mtk_usb (extcon provider)          usb-boost-manager (musb_boost,
mt_usb role switch "mt_usb-role-switch"     "mediatek,mt6768-usb-boost",
                                            OTG 5V boost, DVFS-managed)
charger: SC8989X @ I2C7 0x6A + mm8013 gauge (MT6370 node unbound)
   ↓ PHY_MODE_USB_HOST/DEVICE
mtk-tphy (T-PHY v1 @ 0x11CC0000, port0 @ 0x11CC0800)
   ↓  phys = <&usb2port0 PHY_TYPE_USB2>
mt_usb (MUSB glue, module musb_main) → musb-hdrc core (module musb_hdrc)
   @ 0x11200000, IRQ GIC 129 (SPI 97, level-low)
   ↓
host: musb-hdrc HCD → usb1 root hub
device: /sys/class/udc/musb-hdrc → configfs gadget (g1: adb/mtp/...)
```

## 8. Register reference table

Base for MUSB rows = `0x11200000`; PHY rows = `0x11CC0800` (v1 U2 COM bank,
offset 0x000); wrapper rows = MUSB base. "Live" column: value observed on the
running tablet or in a DT for this SoC; "—" means not observed live.

| Register | Base+offset | Meaning | Android observed | HOST desired | DEVICE desired | Source |
|---|---|---|---|---|---|---|
| `MUSB_FADDR` | +0x00 | device address | 0 (device g1) | n/a | 0 then assigned addr | vendor `mtk_musb_reg.h:206`, U-Boot `mt6768.c` |
| `MUSB_POWER` | +0x01 | power/session/HSMODE | — | HSEN+SESSION via core | SOFT_CONN+EN_SUSP | musb_regs.h; U-Boot init seq |
| `MUSB_INTRTX/E` | +0x02/0x06 | TX irq status/enable | masked via L1 | per-EP enables | per-EP enables | musb_regs.h |
| `MUSB_INTRRX/E` | +0x04/0x08 | RX irq status/enable | — | per-EP enables | per-EP enables | musb_regs.h |
| `MUSB_INTRUSB/E` | +0x0A/0x0B | common irq status/enable | — | RESUME/CONN/DISCON | RESET/SUSPEND/SOF | musb_regs.h |
| `MUSB_FRAME` | +0x0C | frame number | — | read-only | read-only | musb_regs.h |
| `MUSB_INDEX` | +0x0E | EP select (indexed model) | — | set per EP | set per EP | mtk_musb_reg.h |
| `MUSB_DEVCTL` | +0x60 | session/B-device/hostreq | **live 0x99 device-attached, 0x80 unattached, 0x5D host (HM+SESSION+FSDEV+VBUS=0b11)** | SESSION=1 | BDEVICE, SESSION mgmt | live dmesg; U-Boot `mt6768.c` init |
| `MUSB_TXFIFOSZ/RXFIFOSZ` | +0x62/0x63 | per-EP FIFO size | dyn-fifo | configured | configured | mtk_musb_reg.h |
| `MUSB_TXFIFOADD/RXFIFOADD` | +0x64/0x66 | per-EP FIFO addr (>>3) | dyn-fifo | configured | configured | mtk_musb_reg.h |
| `MUSB_HWVERS` | +0x6C | RTL version | — | read-only | read-only | mtk_musb_reg.h |
| `MUSB_EPINFO` | +0x78 | EP count/RAM info | — | read-only | read-only | mtk_musb_reg.h |
| **`MUSB_SWRST`** | **+0x74** | MTK soft reset: PHY_RST b7, PHYSIG_GATE_HS b6, PHYSIG_GATE_EN b5, REDUCE_DLY b4, UNDO_SRPFIX b3, **FRC_VBUSVALID b2**, SWRST b1, DISUSBRESET b0 | — | as needed | as needed | mtk_musb_reg.h:305 |
| **`USB_L1INTS`** | **+0xA0** | L1 int status: TX b0, RX b1, USBCOM b2, DMA b3, QMU b4 | unmasked, firing | poll/poke | poll/poke | mtk_musb_reg.h:312; U-Boot live |
| **`USB_L1INTM`** | **+0xA4** | L1 int mask | `0xF` (no QMU) / `0x1F` (QMU) written at init | TX\|RX\|COM\|DMA | TX\|RX\|COM\|DMA | U-Boot `mt6768.c`; vendor `usb20.c:1626` |
| `USB_L1INTP` | +0xA8 | L1 int polarity | — | verify | verify | mtk_musb_reg.h:314 |
| `MUSB_TXFUNCADDR` | +0x480 (+8*ep) | target function addr (host, multipoint) | — | per-target | n/a | mtk_musb_reg.h:286 |
| `MUSB_TXHUBADDR/RXHUBADDR` | +0x482/0x486 | hub addr (host) | — | for hub use | n/a | mtk_musb_reg.h |
| `MUSB_RXTOG/RXTOGEN` | +0x80/0x82 | RX data toggle (+enable) | EN=0xFFFF | EN bits 15:0 | EN bits 15:0 | mtk_musb_reg.h; U-Boot init |
| `MUSB_TXTOG/TXTOGEN` | +0x84/0x86 | TX data toggle (+enable) | EN=0xFFFF | EN bits 15:0 | EN bits 15:0 | mtk_musb_reg.h; U-Boot init |
| EP regs (indexed `0x10+`) | TXMAXP +0x00, TXCSR +0x02, RXMAXP +0x04, RXCSR +0x06, RXCOUNT +0x08, TXTYPE +0x0A, TXINTERVAL +0x0B, RXTYPE +0x0C, RXINTERVAL +0x0D, CONFIGDATA/ FIFOSIZE +0x0F | per-EP CSR set; live adb: EP1 OUT fifo@512, EP1 IN fifo@1536, FIFOSZ 0x22 (512 B DBBUF) | - | host CSRs + `TXCSR_MODE 0x2000` | device CSRs | live dmesg fifo_setup; mtk_musb_reg.h |
| `MUSB_FIFO(ep)` | +0x20+4*ep | PIO FIFO window | — | 8/16/32-bit acc | 8/16/32-bit acc | mtk_musb_reg.h |
| HSDMA | +0x200 (chan n at 0x200+0x10n), REALCOUNT 0x280+0x10n, intr set bits 24+ | classic MUSB DMA | present | deferred (PIO first) | deferred (PIO first) | mtk_musb_reg.h:315-317 |
| QMU global | `MUSB_USBGCSR` +0xB00, `MUSB_QIMCR` +0xC08, `MUSB_QIMSR` +0xC0C, `MUSB_USB_MDL1INTM` +0x744 | QMU enable/mask | vendor uses QMU | deferred | deferred | mtk_musb_reg.h:232,232-236 |
| QMU rings | RXQCSR(n)=0x0010+0x10(n-1), TXQCSR(n)=0x0200+0x10(n-1) (within QMU page) | GPD ring start/flush | vendor-only | phase 2 | phase 2 (MSD) | mtk_qmu.h:176-195 |
| `U3P_U2PHYDTM0` | PHY+0x68 | force data/pulldown/xcvrsel/termsel/suspendm | — | clear force bits | clear force bits | upstream phy-mtk-tphy.c:92-110 |
| `U3P_U2PHYDTM1` | PHY+0x6C | force IDDIG/VBUSVALID/SESSEND/AVALID/BVALID/IDPULLUP (+FORCE_* bits 8..13) | **live device value 0x3F2F after a `0x6C=0` clear** | `MTK_DTM1_HOST_SET` (vendor recipe) | **`0x3F2F` = `MTK_DTM1_DEVICE_SET` (vendor recipe)** | live dmesg `set_usb_phy_mode 0x6c=3f2f`; vendor `usb20_phy.c:492-517` |
| `U3D_U2PHYDCR0` | PHY+0x60 | `P2C_RG_SIF_U2PLL_FORCE_ON` BIT(24) | — | power-on seq | power-on seq | phy-mtk-tphy.c:59-61 |
| Tuning CRs | PHY+0x0..0x78 | USBPHYACR* / U2PHYACR4 / U2PHYDCR1 / disconnect/eye/term; `usb_rev6_setting` writes 0x18[31:24] | DT values above (barley: disc 7/0xB, eye-term 4/6, rev6 2) | apply DT host values | apply DT device values | vendor `usb20_phy.c:519-532`, `mtk-phy-a60810.h` |

## 9. Open items that block firmware work (and how to close them)

1. **VBUS boost control decode** - the `usb-boost-manager` node is now fully
   known (compatibles + interconnect/OPP wiring) but the *register/GPIO*
   enable sequence inside the `musb_boost` module still needs the module
   internals (root is available; decompile `/vendor/lib/modules/musb_boost.ko`
   or trace it during an OTG test). Until then UEFI must not enable VBUS.
2. **Live clock state at UEFI entry** - read INFRA_CFG_AO 0x90 / CKGEN 0x90
   status from a diagnostic UEFI build (pattern already proven by MSDC).
   Android leaves ICUSB/USB_TOP_SEL enabled and running (156/62.4 MHz) in
   normal operation; the LK-handoff state still needs one probe.
3. **Role-switch entry point in 6.6 musb_main** - resolved in outline by the
   live logs: TCPC -> extcon -> `mailbox VBUS_*` -> `mt_usb_connect/disconnect`
   -> musb_start with `set_usb_phy_mode()` + SOFTCONN. The UEFI device path
   replicates: PHY clear+set (0x6C recipe), `MtkMusbInitInterrupts`, softconn.
4. ~~Root access~~ - **solved**: `adb root` works on this build; all dmesg,
   extcon, TCPC, DTB evidence captured (see UPDATE 2 in the observations doc).
