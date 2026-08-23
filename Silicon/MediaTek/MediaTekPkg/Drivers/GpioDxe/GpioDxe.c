#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/IoLib.h>

#include <Library/GpioImplLib.h>

#include <Protocol/MtkGpio.h>

#define PIN_OFFSET(pin) (((pin) / 32) * 0x10)

#define PIN_MODE_OFFSET(pin) (((pin) / 8) * 0x10)
#define PIN_MODE_BIT(pin) (((pin) % 8) * 4)

STATIC EFI_MEMORY_REGION_DESCRIPTOR mPinctrlRegion;

STATIC
UINT32
GpioRead(
  IN  UINT32 Reg)
{
  return MmioRead32 (mPinctrlRegion.Address + Reg);
}

STATIC
VOID
GpioWrite(
  IN  UINT32 Reg,
  IN  UINT32 Value)
{
  MmioWrite32 (mPinctrlRegion.Address + Reg, Value);
}

EFI_STATUS
GpioGetDir(
  IN  UINT32   Pin,
  OUT BOOLEAN *Direction)
{
  UINT32 Value;

  if ((Pin > gPlatformInfo.MaxPin) || (Direction == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // MediaTek GPIO direction bits are 1 for output and 0 for input.  The
  // protocol exposes TRUE as input, matching SetDir() and its consumers.
  Value = GpioRead (gPlatformInfo.DirOffset + PIN_OFFSET(Pin));
  *Direction = ((Value & (1U << (Pin % 32))) == 0);

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetDir(
  IN UINT32  Pin,
  IN BOOLEAN Direction)
{
  UINT32 Offset;

  if (Pin > gPlatformInfo.MaxPin) {
    return EFI_INVALID_PARAMETER;
  }

  // TRUE means input.  MediaTek's SET/RESET aliases are write-one registers;
  // reading and writing their contents back can affect unrelated pins.
  Offset = gPlatformInfo.DirOffset + PIN_OFFSET (Pin) +
           (Direction ? gPlatformInfo.ResetOffset : gPlatformInfo.SetOffset);
  GpioWrite (Offset, 1U << (Pin % 32));

  return EFI_SUCCESS;
}

EFI_STATUS
GpioGetState(
  IN  UINT32   Pin,
  OUT BOOLEAN *State)
{
  EFI_STATUS Status;
  UINT32 Value;
  BOOLEAN Direction;

  if ((Pin > gPlatformInfo.MaxPin) || (State == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GpioGetDir (Pin, &Direction);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Value = GpioRead ((Direction ? gPlatformInfo.DataInOffset : gPlatformInfo.DataOutOffset) + PIN_OFFSET(Pin));
  *State = ((Value & (1U << (Pin % 32))) != 0);

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetState(
  IN UINT32  Pin,
  IN BOOLEAN State)
{
  UINT32 Offset;

  if (Pin > gPlatformInfo.MaxPin) {
    return EFI_INVALID_PARAMETER;
  }

  Offset = gPlatformInfo.DataOutOffset + (State ? gPlatformInfo.SetOffset : gPlatformInfo.ResetOffset) + PIN_OFFSET(Pin);
  GpioWrite (Offset, 1U << (Pin % 32));

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetMode(
  IN UINT32 Pin,
  IN UINT32 Mode)
{
  UINT32 ModeMask;
  UINT32 Offset;

  if ((Pin > gPlatformInfo.MaxPin) || (Mode > 7)) {
    return EFI_INVALID_PARAMETER;
  }

  Offset = gPlatformInfo.ModeOffset + PIN_MODE_OFFSET (Pin);
  ModeMask = 0x7U << PIN_MODE_BIT (Pin);

  // Program only this pin's three mode bits through the write-one aliases.
  GpioWrite (Offset + gPlatformInfo.ResetOffset, ModeMask);
  GpioWrite (Offset + gPlatformInfo.SetOffset, Mode << PIN_MODE_BIT (Pin));

  return EFI_SUCCESS;
}

STATIC MTK_GPIO_PROTOCOL mGpio = {
  GpioGetDir,
  GpioSetDir,
  GpioGetState,
  GpioSetState,
  GpioSetMode
};

EFI_STATUS
EFIAPI
InitGpioDriver (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  // Locate Pinctrl Memory Region
  Status = LocateMemoryRegionByName ("Pinctrl", &mPinctrlRegion);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate Pinctrl Memory Region! Status = %r\n", Status));
    return Status;
  }

  // Register GPIO Protocol
  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle, &gMediaTekGpioProtocolGuid, &mGpio, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Register Gpio Protocol! Status = %r\n", Status));
    return Status;
  }

  return Status;
}
