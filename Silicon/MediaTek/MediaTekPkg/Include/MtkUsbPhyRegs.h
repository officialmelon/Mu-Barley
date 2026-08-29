/** @file
  Register definitions for the MediaTek T-PHY (generic-tphy-v1) USB2 port
  on MT6768 (Barley / TB330XU).

  The T-PHY shared block (IPPC) is at 0x11CC0000; for tphy-v1 each port node
  register space IS the U2 PHY register bank (SSUSB_SIFSLV_V1_U2PHY_COM =
  0x000 relative to the port base).  Barley port 0 is at 0x11CC0800.

  Sources:
   - Upstream Linux drivers/phy/mediatek/phy-mtk-tphy.c (generic-tphy-v1)
   - Vendor alps drivers/misc/mediatek/usb20/mtk-phy-a60810.h (register
     map of the same USB2 PHY IP generation)
   - U-Boot MT6768 dtsi (Barley-relevant tuning values)

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MTK_USB_PHY_REGS_H_
#define MTK_USB_PHY_REGS_H_

#define MTK_TPHY_IPPC_BASE         0x11CC0000
#define MTK_TPHY_IPPC_LENGTH       SIZE_2KB

#define MTK_TPHY_U2_PORT0_BASE     0x11CC0800
#define MTK_TPHY_U2_PORT_LENGTH    SIZE_256B

/* tphy-v1: port register base equals the U2PHY COM bank. */
#define SSUSB_SIFSLV_V1_U2PHY_COM  0x000

/*
 * Barley U2 PHY tuning from the LIVE TB330XU devicetree (not lancelot —
 * the values differ): device disc=7 / host disc=0xB, eye-vrt 7 both,
 * eye-term 4 device / 6 host, rev6 2 both.  The port also carries an
 * efuse "intr_cal" cell (mask 0x1F) applied by the driver.
 */
#define MTK_U2PHY_DISCTH_DEVICE    0x07
#define MTK_U2PHY_DISCTH_HOST      0x0B
#define MTK_U2PHY_EYE_VRT          0x07
#define MTK_U2PHY_EYE_TERM_DEVICE  0x04
#define MTK_U2PHY_EYE_TERM_HOST    0x06
#define MTK_U2PHY_REV6             0x02

/* U2PHY DC registers */
#define U3D_U2PHYDCR0              0x060
#define   P2C_RG_SIF_U2PLL_FORCE_ON  (1 << 24)
#define   P2C_RG_DATA_PIN            (1 << 20)
#define   P2C_RG_CLK60_EN            (1 << 8)

#define U3P_U2PHYDCR1              0x064

/* U2PHY DT registers */
#define U3P_U2PHYDTM0              0x068
#define   P2C_FORCE_UART_EN          (1 << 26)
#define   P2C_FORCE_DATAIN           (1 << 23)
#define   P2C_FORCE_DM_PULLDOWN      (1 << 21)
#define   P2C_FORCE_DP_PULLDOWN      (1 << 20)
#define   P2C_FORCE_XCVRSEL          (1 << 19)
#define   P2C_FORCE_SUSPENDM         (1 << 18)
#define   P2C_FORCE_TERMSEL          (1 << 17)
#define   P2C_RG_DATAIN              (0xF << 10)
#define   P2C_RG_DATAIN_SHIFT        10
#define   P2C_RG_DMPULLDOWN          (1 << 7)
#define   P2C_RG_DPPULLDOWN          (1 << 6)
#define   P2C_RG_XCVRSEL             (0x3 << 4)
#define   P2C_RG_XCVRSEL_SHIFT       4
#define   P2C_RG_SUSPENDM            (1 << 3)
#define   P2C_RG_TERMSEL             (1 << 2)
#define P2C_DTM0_PART_MASK         \
  (P2C_FORCE_DATAIN | P2C_FORCE_DM_PULLDOWN | \
   P2C_FORCE_DP_PULLDOWN | P2C_FORCE_XCVRSEL | \
   P2C_FORCE_TERMSEL | P2C_RG_DMPULLDOWN | \
   P2C_RG_DPPULLDOWN | P2C_RG_TERMSEL)

#define U3P_U2PHYDTM1              0x06C
/*
 * DTM1 bit map.  The A60810 vendor names are authoritative for this
 * silicon (they are what the live vendor module writes); the upstream
 * tphy P2C_* names agree wherever both define a bit.
 * Live confirmation: device mode on barley leaves DTM1 = 0x3F2F
 * ("force PHY to mode 6, 0x6c=3f2f", vendor musb_main).
 */
#define   P2C_RG_IDPULLUP            (1 << 0)  /* A60810_RG_IDPULLUP   */
#define   P2C_RG_IDDIG               (1 << 1)  /* = P2C_RG_IDDIG       */
#define   P2C_RG_AVALID              (1 << 2)  /* = P2C_RG_AVALID      */
#define   P2C_RG_BVALID              (1 << 3)  /* A60810_RG_BVALID     */
#define   P2C_RG_SESSEND             (1 << 4)  /* = P2C_RG_SESSEND     */
#define   P2C_RG_VBUSVALID           (1 << 5)  /* = P2C_RG_VBUSVALID   */
#define   P2C_FORCE_IDPULLUP         (1 << 8)  /* A60810_FORCE_IDPULLUP */
#define   P2C_FORCE_IDDIG            (1 << 9)  /* = P2C_FORCE_IDDIG    */
#define   P2C_FORCE_AVALID           (1 << 10) /* A60810_FORCE_AVALID  */
#define   P2C_FORCE_BVALID           (1 << 11) /* A60810_FORCE_BVALID  */
#define   P2C_FORCE_SESSEND          (1 << 12) /* A60810_FORCE_SESSEND */
#define   P2C_FORCE_VBUSVALID        (1 << 13) /* A60810_FORCE_VBUSVALID */
#define   P2C_FORCE_IP_U2_PORT_POWER (1 << 14)

/* Vendor role-select sequences on DTM1 (usb20_phy.c set_usb_phy_mode),
 * exactly as executed on barley: */
#define MTK_DTM1_FORCE_ALL         \
  (P2C_FORCE_IDPULLUP | P2C_FORCE_IDDIG | P2C_FORCE_AVALID | \
   P2C_FORCE_BVALID | P2C_FORCE_SESSEND | P2C_FORCE_VBUSVALID)
/* PHY_DEV_ACTIVE (device): observed live value 0x3F2F */
#define MTK_DTM1_DEVICE_SET        \
  (MTK_DTM1_FORCE_ALL | P2C_RG_IDPULLUP | P2C_RG_IDDIG | \
   P2C_RG_AVALID | P2C_RG_BVALID | P2C_RG_VBUSVALID)
#define MTK_DTM1_DEVICE_CLR        (P2C_RG_SESSEND)
/* PHY_HOST_ACTIVE (host): clear IDDIG+SESSEND, set the rest */
#define MTK_DTM1_HOST_SET          \
  (MTK_DTM1_FORCE_ALL | P2C_RG_IDPULLUP | \
   P2C_RG_AVALID | P2C_RG_BVALID | P2C_RG_VBUSVALID)
#define MTK_DTM1_HOST_CLR          (P2C_RG_IDDIG | P2C_RG_SESSEND)
/* PHY_IDLE_MODE */
#define MTK_DTM1_IDLE_SET          (MTK_DTM1_FORCE_ALL | P2C_RG_IDPULLUP | P2C_RG_SESSEND)
#define MTK_DTM1_IDLE_CLR         \
  (P2C_RG_IDDIG | P2C_RG_AVALID | P2C_RG_BVALID | P2C_RG_VBUSVALID)

/* U3D_U2PHYDMON1 readback status (vendor usb20_phy debug) */
#define U3P_U2PHYDMON1             0x074
#define   A60810_USB20_SESSEND       (1 << 25)
#define   A60810_USB20_VBUSVALID     (1 << 26)
#define   A60810_USB20_IDDIG         (1 << 27)
#define   A60810_USB20_BVALID        (1 << 28)

#define U3P_U2PHYBC12C             0x080
#define   P2C_RG_CHGDT_EN            (1 << 0)

/*
 * Role-select: use the vendor set_usb_phy_mode() sequences defined above
 * (clear-then-set on DTM1).  The upstream tphy subset (FORCE_IDDIG +
 * RG_IDDIG only, VBUSVALID/AVALID/SESSEND via power_on/off) is a subset of
 * these and is consistent with them.  Barley live evidence: after Android
 * brings the device role up, DTM1 = 0x3F2F = MTK_DTM1_DEVICE_SET &
 * ~(MTK_DTM1_DEVICE_CLR) with the high force byte set.
 */

#endif /* MTK_USB_PHY_REGS_H_ */
