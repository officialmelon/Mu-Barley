/** @file
  Barley (TB330XU) board-specific USB port policy.

  Everything board-specific about the single USB-C port lives here:
  the RT1711H TCPC description, the usb-boost VBUS manager, charger
  interaction and role policy.  Generic MT6768 code must never embed
  these values.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef BARLEY_USB_PORT_LIB_H_
#define BARLEY_USB_PORT_LIB_H_

#include <Uefi.h>

/*
 * Board facts established live (Docs/USB-HARDWARE-MANIFEST.md):
 *   TCPC      : Richtek RT1711H at I2C controller 0x1100D000, address 0x4E,
 *               interrupt on EINT 41 (level).
 *   VBUS sink : charger framework reports USB SDP/DCP/CDP/PD.
 *   VBUS src  : "usb-boost-manager" (module musb_boost) - enable method
 *               NOT yet decoded; this library refuses to drive it.
 */
#define BARLEY_TCPC_I2C_CONTROLLER_BASE  0x1100D000
#define BARLEY_TCPC_I2C_ADDRESS          0x4E
#define BARLEY_TCPC_EINT                 41

typedef enum {
  BarleyUsbRoleUnspecified,
  BarleyUsbRoleDevice,     /* attached to a PC (CC Rd seen by partner)   */
  BarleyUsbRoleHost,       /* OTG adapter / partner Rp (requires VBUS!)  */
} BARLEY_USB_ROLE;

typedef struct {
  BOOLEAN          VbusBoostDecoded;   /* FALSE until usb-boost is decoded */
  BARLEY_USB_ROLE  InitialRole;
} BARLEY_USB_PORT_POLICY;

/**
  Return the bring-up role policy.  The initial role for UEFI shell
  bring-up is DEVICE: it needs no VBUS sourcing and matches the state
  Android leaves the hardware in.
**/
CONST BARLEY_USB_PORT_POLICY *
BarleyUsbGetPortPolicy (
  VOID
  );

/**
  Attempt to enable the 5 V boost for host-mode VBUS.

  Returns EFI_UNSUPPORTED until the usb-boost-manager enable path is
  decoded from the vendor module; this guard exists so that no caller can
  accidentally source VBUS with unknown polarity or gating.
**/
EFI_STATUS
BarleyUsbEnableVbusBoost (
  VOID
  );

#endif /* BARLEY_USB_PORT_LIB_H_ */
