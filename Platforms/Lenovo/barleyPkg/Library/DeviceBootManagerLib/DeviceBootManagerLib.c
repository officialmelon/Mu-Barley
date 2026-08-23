/** @file
  Minimal Barley boot policy: launch the FV-resident UEFI Shell after console
  connection.  Peripheral boot policy is intentionally deferred until the
  core platform is proven.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/DeviceBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/SdMmcPassThru.h>
#include <Protocol/SimpleFileSystem.h>

extern EFI_GUID gUefiShellFileGuid;

STATIC
UINTN
CountProtocolHandles (
  IN EFI_GUID *Protocol
  )
{
  EFI_HANDLE *Handles;
  EFI_STATUS  Status;
  UINTN       HandleCount;

  Handles = NULL;
  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  Protocol,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return 0;
  }

  FreePool (Handles);
  return HandleCount;
}

STATIC
EFI_STATUS
ConnectEmmcPassThruControllers (
  OUT UINTN *ControllerCount
  )
{
  EFI_HANDLE *Handles;
  EFI_STATUS  ConnectStatus;
  EFI_STATUS  Status;
  UINTN       HandleCount;
  UINTN       Index;

  if (ControllerCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ControllerCount = 0;

  // BDS is in APRIORI, while the deliberately minimal storage stack is not.
  // Dispatch the remaining FV drivers before looking for MSDC pass-through
  // controller handles.  Do not recursively connect unrelated controllers.
  do {
    Status = gDS->Dispatch ();
  } while (!EFI_ERROR (Status));

  Handles     = NULL;
  HandleCount = 0;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSdMmcPassThruProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *ControllerCount = HandleCount;
  ConnectStatus    = EFI_NOT_FOUND;
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
    if (!EFI_ERROR (Status)) {
      ConnectStatus = EFI_SUCCESS;
    } else if (ConnectStatus == EFI_NOT_FOUND) {
      ConnectStatus = Status;
    }
  }

  FreePool (Handles);
  return ConnectStatus;
}

STATIC
EFI_STATUS
BuildShellLoadOption (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION *LoadOption
  )
{
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  EFI_LOADED_IMAGE_PROTOCOL         *LoadedImage;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_STATUS                         Status;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EfiInitializeFwVolDevicepathNode (&FileNode, &gUefiShellFileGuid);
  DevicePath = AppendDevicePathNode (
                 DevicePathFromHandle (LoadedImage->DeviceHandle),
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EfiBootManagerInitializeLoadOption (
             LoadOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP,
             L"UEFI Shell",
             DevicePath,
             NULL,
             0
             );
  FreePool (DevicePath);
  return Status;
}

EFI_HANDLE
EFIAPI
DeviceBootManagerBeforeConsole (
  OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath,
  OUT BDS_CONSOLE_CONNECT_ENTRY **PlatformConsoles
  )
{
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerAfterConsole (
  VOID
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION ShellOption = {0};
  EFI_STATUS                   ConnectStatus;
  UINTN                        ControllerCount;

  if (gST->ConOut != NULL) {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
    gST->ConOut->ClearScreen (gST->ConOut);
    gST->ConOut->OutputString (
                   gST->ConOut,
                   L"Barley UEFI\r\nGOP and DXE OK\r\nProbing LK-inherited eMMC host 0...\r\n"
                   );
  }

  ControllerCount = 0;
  ConnectStatus = ConnectEmmcPassThruControllers (&ControllerCount);

  if (gST->ConOut != NULL) {
    Print (
      L"eMMC proof: %u pass-through, %u Block I/O, %u Disk I/O, "
      L"%u partitions, %u file systems, connect %r\r\n"
      L"Launching FV UEFI Shell...\r\n",
      ControllerCount,
      CountProtocolHandles (&gEfiBlockIoProtocolGuid),
      CountProtocolHandles (&gEfiDiskIoProtocolGuid),
      CountProtocolHandles (&gEfiPartitionInfoProtocolGuid),
      CountProtocolHandles (&gEfiSimpleFileSystemProtocolGuid),
      ConnectStatus
      );
  }

  if (!EFI_ERROR (BuildShellLoadOption (&ShellOption))) {
    EfiBootManagerBoot (&ShellOption);
    EfiBootManagerFreeLoadOption (&ShellOption);
  }

  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerOnDemandConInConnect (
  VOID
  )
{
  return NULL;
}

VOID
EFIAPI
DeviceBootManagerProcessBootCompletion (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
  )
{
}

EFI_STATUS
EFIAPI
DeviceBootManagerPriorityBoot (
  IN OUT EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
  )
{
  return EFI_NOT_FOUND;
}

VOID
EFIAPI
DeviceBootManagerUnableToBoot (
  VOID
  )
{
}

VOID
EFIAPI
DeviceBootManagerBdsEntry (
  VOID
  )
{
}
