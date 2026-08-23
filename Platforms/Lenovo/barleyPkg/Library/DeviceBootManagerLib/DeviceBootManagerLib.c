/** @file
  Minimal Barley boot policy: launch the FV-resident UEFI Shell after console
  connection.  Peripheral boot policy is intentionally deferred until the
  core platform is proven.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/DeviceBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SdMmcPassThru.h>

extern EFI_GUID gUefiShellFileGuid;

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

STATIC
VOID
ConnectEmmcControllers (
  VOID
  )
{
  EFI_HANDLE *Handles;
  UINTN       HandleCount;
  UINTN       Index;
  EFI_STATUS  Status;

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
    return;
  }

  // EmmcDxe is a driver-binding driver.  Connect only the controllers created
  // by MsdcDxe so this milestone cannot start unrelated platform hardware.
  for (Index = 0; Index < HandleCount; Index++) {
    gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
  }

  FreePool (Handles);
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

  ConnectEmmcControllers ();

  if (gST->ConOut != NULL) {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
    gST->ConOut->ClearScreen (gST->ConOut);
    gST->ConOut->OutputString (
                   gST->ConOut,
                   L"Barley UEFI\r\nGOP and DXE OK\r\nLaunching UEFI Shell...\r\n"
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
