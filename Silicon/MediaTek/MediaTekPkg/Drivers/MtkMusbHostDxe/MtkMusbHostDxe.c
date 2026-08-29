/** @file
  MediaTek MUSB host controller driver skeleton (EFI_USB2_HC_PROTOCOL).

  Bring-up scope (Docs/USB-HARDWARE-MANIFEST.md):
    1. PIO-only control/bulk/interrupt transfers, no DMA, no isochronous.
    2. Root hub emulation compatible with UsbBusDxe.
    3. First milestone: USB keyboard in the UEFI shell.

  This revision registers nothing and performs no hardware writes: it is
  the compile-only scaffold the real driver grows out of.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/Usb2HostController.h>

#include <Library/MtkMusbCoreLib.h>
#include <Library/MtkUsbPhyLib.h>

#define MUSB_HOST_DXE_SIGNATURE  SIGNATURE_32 ('M', 'T', 'M', 'H')

typedef struct {
  UINT32                    Signature;
  EFI_USB2_HC_PROTOCOL      Usb2Hc;
  EFI_HANDLE                ControllerHandle;
  MTK_USB_PHY_CONTEXT       Phy;
  UINTN                     MusbBase;
} MTK_MUSB_HOST_DEV;

STATIC MTK_MUSB_HOST_DEV  mMtkMusbHostDev;

STATIC
EFI_STATUS
EFIAPI
MusbHostGetCapability (
  IN  EFI_USB2_HC_PROTOCOL  *This,
  OUT UINT8                 *MaxSpeed,
  OUT UINT8                 *PortNumber,
  OUT UINT8                 *Is64BitCapable
  )
{
  if ((MaxSpeed == NULL) || (PortNumber == NULL) || (Is64BitCapable == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *MaxSpeed       = EFI_USB_SPEED_HIGH;  /* MUSB HS (480 Mbps) */
  *PortNumber     = 1;                                /* single root port   */
  *Is64BitCapable = 0;                                /* 32-bit DMA, PIO    */

  return EFI_SUCCESS;
}

/*
 * Protocol template.  Only GetCapability is implemented in the scaffold;
 * every other operation is deliberately EFI_UNSUPPORTED until the PIO
 * transfer engine passes its bring-up milestones.
 */
STATIC EFI_USB2_HC_PROTOCOL  mUsb2HcTemplate = {
  MusbHostGetCapability,                      /* GetCapability            */
  (EFI_USB2_HC_PROTOCOL_RESET)NULL,           /* Reset                    */
  (EFI_USB2_HC_PROTOCOL_GET_STATE)NULL,       /* GetState                 */
  (EFI_USB2_HC_PROTOCOL_SET_STATE)NULL,       /* SetState                 */
  (EFI_USB2_HC_PROTOCOL_CONTROL_TRANSFER)NULL,/* ControlTransfer          */
  (EFI_USB2_HC_PROTOCOL_BULK_TRANSFER)NULL,   /* BulkTransfer             */
  (EFI_USB2_HC_PROTOCOL_ASYNC_INTERRUPT_TRANSFER)NULL, /* AsyncInterruptTransfer   */
  (EFI_USB2_HC_PROTOCOL_SYNC_INTERRUPT_TRANSFER)NULL,  /* SyncInterruptTransfer    */
  (EFI_USB2_HC_PROTOCOL_ISOCHRONOUS_TRANSFER)NULL,     /* IsochronousTransfer      */
  (EFI_USB2_HC_PROTOCOL_ASYNC_ISOCHRONOUS_TRANSFER)NULL,/* AsyncIsochronousTransfer*/
  (EFI_USB2_HC_PROTOCOL_GET_ROOTHUB_PORT_STATUS)NULL,  /* GetRootHubPortStatus     */
  (EFI_USB2_HC_PROTOCOL_SET_ROOTHUB_PORT_FEATURE)NULL, /* SetRootHubPortFeature    */
  (EFI_USB2_HC_PROTOCOL_CLEAR_ROOTHUB_PORT_FEATURE)NULL,/*ClearRootHubPortFeature */
  0x0001,                    /* MajorRevision: USB 2.0 spec */
  0x0000                     /* MinorRevision               */
};

EFI_STATUS
EFIAPI
MtkMusbHostDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  /*
   * Scaffold only: the driver must stay out of the platform DSC/FDF until
   * the hardware bring-up milestones (clock state probe, IRQ attach,
   * EP0 control transfer) have been passed.  Nothing is installed here.
   */
  mMtkMusbHostDev.Signature = MUSB_HOST_DXE_SIGNATURE;
  mMtkMusbHostDev.MusbBase  = MTK_MUSB_BASE;

  DEBUG ((
    DEBUG_INFO,
    "%a: scaffold present, controller at 0x%lx (not started), template %p\n",
    __FUNCTION__,
    (UINT64)mMtkMusbHostDev.MusbBase,
    &mUsb2HcTemplate
    ));

  return EFI_SUCCESS;
}
