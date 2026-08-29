/** @file
  MediaTek MUSB device (gadget) controller driver skeleton.

  First device-mode milestone: the desktop PC enumerates a valid dummy USB
  device.  Scope of the eventual driver (Docs/USB-HARDWARE-MANIFEST.md):
    - EP0 SETUP / DATA / STATUS handling with the vendor W1C conventions
    - endpoint configure/enable/disable, IN/OUT transfers, stall
    - address set, configuration state, connect/disconnect (soft connect
      through MUSB_POWER_SOFTCONN)
    - an EFI_USBFN_IO_PROTOCOL-compatible surface so the existing Silicium
      EFI_USB_MSD_PROTOCOL stack can consume it later

  This revision performs no hardware writes and installs nothing.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>

#include <Library/MtkMusbCoreLib.h>
#include <Library/MtkUsbPhyLib.h>

#define MUSB_DEVICE_DXE_SIGNATURE  SIGNATURE_32 ('M', 'T', 'M', 'D')

typedef struct {
  UINT32                Signature;
  UINTN                 MusbBase;
  MTK_USB_PHY_CONTEXT   Phy;
  UINT8                 Address;
  BOOLEAN               Configured;
} MTK_MUSB_DEVICE_DEV;

STATIC MTK_MUSB_DEVICE_DEV  mMtkMusbDeviceDev;

EFI_STATUS
EFIAPI
MtkMusbDeviceDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  /*
   * Scaffold only: stays out of the platform DSC/FDF until EP0
   * enumeration is proven reliable.  No protocol installation, no
   * hardware writes, no VBUS/clock changes.
   */
  mMtkMusbDeviceDev.Signature  = MUSB_DEVICE_DXE_SIGNATURE;
  mMtkMusbDeviceDev.MusbBase   = MTK_MUSB_BASE;
  mMtkMusbDeviceDev.Address    = 0;
  mMtkMusbDeviceDev.Configured = FALSE;

  DEBUG ((
    DEBUG_INFO,
    "%a: scaffold present, controller at 0x%lx (not started)\n",
    __FUNCTION__,
    (UINT64)mMtkMusbDeviceDev.MusbBase
    ));

  return EFI_SUCCESS;
}
