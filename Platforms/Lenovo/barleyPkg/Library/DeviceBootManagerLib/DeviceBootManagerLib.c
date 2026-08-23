/** @file
  Barley device boot hooks.  Normal media selection is handled by Silicium's
  standard boot policy; the FV-resident UEFI Shell is retained only as the
  final recovery console when no bootable media is found.

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
  EFI_BOOT_MANAGER_LOAD_OPTION ShellOption = {0};

  Print (L"No bootable media found; launching the recovery UEFI Shell...\r\n");
  if (!EFI_ERROR (BuildShellLoadOption (&ShellOption))) {
    EfiBootManagerBoot (&ShellOption);
    EfiBootManagerFreeLoadOption (&ShellOption);
  }
}

VOID
EFIAPI
DeviceBootManagerBdsEntry (
  VOID
  )
{
}
