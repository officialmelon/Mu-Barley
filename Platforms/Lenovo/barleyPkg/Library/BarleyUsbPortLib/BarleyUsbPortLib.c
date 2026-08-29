/** @file
  Barley (TB330XU) board-specific USB port policy.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include "BarleyUsbPortLib.h"

STATIC CONST BARLEY_USB_PORT_POLICY  mBarleyPortPolicy = {
  FALSE,                   /* VbusBoostDecoded */
  BarleyUsbRoleDevice      /* InitialRole      */
};

CONST BARLEY_USB_PORT_POLICY *
BarleyUsbGetPortPolicy (
  VOID
  )
{
  return &mBarleyPortPolicy;
}

EFI_STATUS
BarleyUsbEnableVbusBoost (
  VOID
  )
{
  DEBUG ((
    DEBUG_ERROR,
    "%a: refused - usb-boost-manager enable path is not decoded yet\n",
    __FUNCTION__
    ));

  return EFI_UNSUPPORTED;
}
