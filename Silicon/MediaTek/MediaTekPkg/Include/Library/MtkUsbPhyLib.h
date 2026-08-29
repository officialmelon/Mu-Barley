/** @file
  Public interface of the MediaTek T-PHY USB2 PHY library.

  Generic MT6768-family T-PHY (generic-tphy-v1) handling.  Board-specific
  behavior (VBUS sourcing, charger interaction, connector orientation)
  belongs in the platform port library, not here.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MTK_USB_PHY_LIB_H_
#define MTK_USB_PHY_LIB_H_

#include <Uefi.h>

typedef enum {
  MtkUsbPhyModeInvalid,
  MtkUsbPhyModeDevice,
  MtkUsbPhyModeHost,
  MtkUsbPhyModeOtg,
} MTK_USB_PHY_MODE;

typedef struct {
  UINTN  IppcBase;    /* 0x11CC0000 shared block */
  UINTN  PortBase;    /* 0x11CC0800 U2 port bank */
  UINT8  PortIndex;
  BOOLEAN Probed;
} MTK_USB_PHY_CONTEXT;

/**
  Probe the PHY: map the fixed bases and snapshot current state without
  writing anything.  Safe to call before clocks are known-good.
**/
EFI_STATUS MtkUsbPhyProbe (MTK_USB_PHY_CONTEXT *Context);

/**
  Read-only status dump of the U2 port bank (bring-up diagnostics).
**/
VOID       MtkUsbPhyPrintStatus (CONST MTK_USB_PHY_CONTEXT *Context);

/**
  Role select via U2PHYDTM1 force bits.  Does not touch clocks or VBUS.
**/
EFI_STATUS MtkUsbPhySetMode (MTK_USB_PHY_CONTEXT *Context, MTK_USB_PHY_MODE Mode);

/**
  Power on/off sequences (suspendm/pulldown handling).  The board port
  library must guarantee clocks and VBUS policy before Host mode use.
**/
EFI_STATUS MtkUsbPhyPowerOn  (MTK_USB_PHY_CONTEXT *Context, MTK_USB_PHY_MODE Mode);
EFI_STATUS MtkUsbPhyPowerOff (MTK_USB_PHY_CONTEXT *Context);

/**
  Apply the Barley DT tuning values (discth/eye-vrt/eye-term/rev6 and the
  host variants).  Values are the ones captured from the device trees.
**/
EFI_STATUS MtkUsbPhyApplyTuning (MTK_USB_PHY_CONTEXT *Context, MTK_USB_PHY_MODE Mode);

#endif /* MTK_USB_PHY_LIB_H_ */
