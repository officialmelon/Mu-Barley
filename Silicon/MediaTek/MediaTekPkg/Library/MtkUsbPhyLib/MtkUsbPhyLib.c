/** @file
  MediaTek T-PHY USB2 PHY library (generic-tphy-v1, MT6768).

  Register semantics follow upstream Linux drivers/phy/mediatek/
  phy-mtk-tphy.c (tphy-v1 U2 bank); register map is defined in
  MtkUsbPhyRegs.h.  Deliberately clock-free and VBUS-free: see the
  BarleyUsbPortLib for board policy.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>

#include <Library/MtkUsbPhyLib.h>
#include <MtkUsbPhyRegs.h>

STATIC CONST CHAR8 *mModeName[] = {
  "invalid",
  "device",
  "host",
  "otg"
};

EFI_STATUS
MtkUsbPhyProbe (
  MTK_USB_PHY_CONTEXT *Context
  )
{
  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Context->IppcBase  = MTK_TPHY_IPPC_BASE;
  Context->PortBase  = MTK_TPHY_U2_PORT0_BASE;
  Context->PortIndex = 0;
  Context->Probed    = TRUE;

  DEBUG ((
    DEBUG_INFO,
    "%a: tphy ippc=0x%lx port0=0x%lx dtm0=%08x dtm1=%08x\n",
    __FUNCTION__,
    (UINT64)Context->IppcBase,
    (UINT64)Context->PortBase,
    MmioRead32 (Context->PortBase + U3P_U2PHYDTM0),
    MmioRead32 (Context->PortBase + U3P_U2PHYDTM1)
    ));

  return EFI_SUCCESS;
}

VOID
MtkUsbPhyPrintStatus (
  CONST MTK_USB_PHY_CONTEXT *Context
  )
{
  if ((Context == NULL) || !Context->Probed) {
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: U2 port bank dump (port0)\n", __FUNCTION__));
  DEBUG ((
    DEBUG_INFO,
    "%a: DCR0=%08x DCR1=%08x DTM0=%08x DTM1=%08x BC12C=%08x\n",
    __FUNCTION__,
    MmioRead32 (Context->PortBase + U3D_U2PHYDCR0),
    MmioRead32 (Context->PortBase + U3P_U2PHYDCR1),
    MmioRead32 (Context->PortBase + U3P_U2PHYDTM0),
    MmioRead32 (Context->PortBase + U3P_U2PHYDTM1),
    MmioRead32 (Context->PortBase + U3P_U2PHYBC12C)
    ));
}

EFI_STATUS
MtkUsbPhySetMode (
  MTK_USB_PHY_CONTEXT *Context,
  MTK_USB_PHY_MODE    Mode
  )
{
  UINT32 Tmp;
  UINT32 Set;
  UINT32 Clr;

  if ((Context == NULL) || !Context->Probed) {
    return EFI_NOT_READY;
  }

  /* Vendor set_usb_phy_mode() sequences, live-confirmed on barley: the
   * device-active value observed in the kernel log is DTM1 = 0x3F2F. */
  switch (Mode) {
    case MtkUsbPhyModeDevice:
      Set = MTK_DTM1_DEVICE_SET;
      Clr = MTK_DTM1_DEVICE_CLR;
      break;
    case MtkUsbPhyModeHost:
      Set = MTK_DTM1_HOST_SET;
      Clr = MTK_DTM1_HOST_CLR;
      break;
    case MtkUsbPhyModeOtg:
      Set = MTK_DTM1_IDLE_SET;
      Clr = MTK_DTM1_IDLE_CLR;
      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  Tmp = MmioRead32 (Context->PortBase + U3P_U2PHYDTM1);
  Tmp = (Tmp & ~Clr) | Set;
  MmioWrite32 (Context->PortBase + U3P_U2PHYDTM1, Tmp);

  DEBUG ((
    DEBUG_INFO,
    "%a: mode=%a dtm1=%08x\n",
    __FUNCTION__,
    mModeName[Mode],
    Tmp
    ));

  return EFI_SUCCESS;
}

EFI_STATUS
MtkUsbPhyPowerOn (
  MTK_USB_PHY_CONTEXT *Context,
  MTK_USB_PHY_MODE    Mode
  )
{
  UINT32 Tmp;

  if ((Context == NULL) || !Context->Probed) {
    return EFI_NOT_READY;
  }

  /* Vendor usb_phy_recover() analog: release the suspend force and the
   * line/term/pulldown forces.  DTM1 session signalling is owned by
   * MtkUsbPhySetMode(). */
  (VOID)Mode;
  Tmp = MmioRead32 (Context->PortBase + U3P_U2PHYDTM0);
  Tmp &= ~P2C_DTM0_PART_MASK;
  Tmp &= ~P2C_FORCE_SUSPENDM;
  Tmp &= ~P2C_RG_SUSPENDM;
  MmioWrite32 (Context->PortBase + U3P_U2PHYDTM0, Tmp);

  return EFI_SUCCESS;
}

EFI_STATUS
MtkUsbPhyPowerOff (
  MTK_USB_PHY_CONTEXT *Context
  )
{
  UINT32 Tmp;

  if ((Context == NULL) || !Context->Probed) {
    return EFI_NOT_READY;
  }

  /* Vendor usb_phy_savecurrent_internal() tail: suspendm low, idle
   * session signalling on DTM1, U2PLL force-off on DCR0. */
  Tmp = MmioRead32 (Context->PortBase + U3P_U2PHYDTM0);
  Tmp &= ~P2C_RG_SUSPENDM;
  MmioWrite32 (Context->PortBase + U3P_U2PHYDTM0, Tmp);

  Tmp = MmioRead32 (Context->PortBase + U3P_U2PHYDTM1);
  Tmp = (Tmp & ~MTK_DTM1_IDLE_CLR) | MTK_DTM1_IDLE_SET;
  MmioWrite32 (Context->PortBase + U3P_U2PHYDTM1, Tmp);

  Tmp = MmioRead32 (Context->PortBase + U3D_U2PHYDCR0);
  Tmp &= ~P2C_RG_SIF_U2PLL_FORCE_ON;
  MmioWrite32 (Context->PortBase + U3D_U2PHYDCR0, Tmp);

  return EFI_SUCCESS;
}

EFI_STATUS
MtkUsbPhyApplyTuning (
  MTK_USB_PHY_CONTEXT *Context,
  MTK_USB_PHY_MODE    Mode
  )
{
  if ((Context == NULL) || !Context->Probed) {
    return EFI_NOT_READY;
  }

  /*
   * The device-tree tuning (discth/eye-vrt/eye-term/rev6) is applied by
   * the vendor driver to USBPHYACR/U2PHYACR/U2PHYD registers whose exact
   * bank addresses for this IP revision still need confirmation against
   * the a60810 register map before any write is made.  This skeleton only
   * reports the values it would apply; wiring the writes is deferred to
   * the bring-up milestone that has live register traces to compare.
   */
  DEBUG ((
    DEBUG_INFO,
    "%a: mode=%a tuning disc=%02x eye_vrt=%02x eye_term=%02x rev6=%02x (deferred)\n",
    __FUNCTION__,
    mModeName[Mode],
    (Mode == MtkUsbPhyModeHost) ? MTK_U2PHY_DISCTH_HOST : MTK_U2PHY_DISCTH_DEVICE,
    MTK_U2PHY_EYE_VRT,
    (Mode == MtkUsbPhyModeHost) ? MTK_U2PHY_EYE_TERM_HOST : MTK_U2PHY_EYE_TERM_DEVICE,
    MTK_U2PHY_REV6
    ));

  return EFI_SUCCESS;
}
