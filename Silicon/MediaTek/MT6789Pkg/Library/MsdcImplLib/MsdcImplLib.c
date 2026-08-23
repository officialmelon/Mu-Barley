#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/MsdcImplLib.h>

#include <Protocol/MtkGpio.h>
#include <Protocol/MtkClock.h>
#include <Protocol/MtkPmic.h>

MSDC_PLATFORM_INFO gPlatformInfo = {
  .NumberOfHosts = 2,
  .UseTop = TRUE,
  .MsdcPadTuneReg = 0xf0,
  .TuningStep = {32, 64},
  .AsyncFifo = TRUE,
  .BusyCheck = TRUE,
  .StopClkFix = TRUE,
  .EnhanceRx = TRUE
};

STATIC MTK_GPIO_PROTOCOL *mGpio = NULL;
STATIC MTK_CLOCK_PROTOCOL *mClock = NULL;
STATIC MTK_PMIC_PROTOCOL *mPmic = NULL;

#define MSDC_SUPPLY_VOLTAGE_UV  3300000

EFI_STATUS
GetSourceClockRate (
  IN  UINT32 Index,
  OUT UINTN *Hz)
{
  EFI_STATUS Status;
  UINT32 ClockId;

  if ((Index >= gPlatformInfo.NumberOfHosts) || (Hz == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (mClock == NULL) {
    return EFI_NOT_READY;
  }

  *Hz = 0;

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Clock Id! Status = %r\n", Status));
    return Status;
  }

  // Get Clock Frequency
  Status = mClock->GetFrequency(ClockId, Hz);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Clock Frequency! Status = %r\n", Status));
  }

  return Status;
}

EFI_STATUS
SourceClockControl (
  IN UINT32  Index,
  IN BOOLEAN Enable)
{
  EFI_STATUS Status;
  UINT32 ClockId;

  if (Index >= gPlatformInfo.NumberOfHosts) {
    return EFI_INVALID_PARAMETER;
  }

  if (mClock == NULL) {
    return EFI_NOT_READY;
  }

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "IFRAO_MSDC0_SRC" : "IFRAO_MSDC1_SRC", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Clock Id! Status = %r\n", Status));
    return Status;
  }

  // Enable Clock
  Status = mClock->SetEnable(ClockId, Enable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Enable Clock! Status = %r\n", Status));
  }

  return Status;
}

EFI_STATUS
ClockControl (
  IN UINT32  Index,
  IN BOOLEAN Enable)
{
  EFI_STATUS Status;
  UINT32 ClockId;

  if (Index >= gPlatformInfo.NumberOfHosts) {
    return EFI_INVALID_PARAMETER;
  }

  if (mClock == NULL) {
    return EFI_NOT_READY;
  }

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Clock Id! Status = %r\n", Status));
    return Status;
  }

  // Enable Clock
  Status = mClock->SetEnable(ClockId, Enable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Enable Clock! Status = %r\n", Status));
    return Status;
  }

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "IFRAO_MSDC0" : "IFRAO_MSDC1", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Clock Id! Status = %r\n", Status));
    return Status;
  }

  // Enable Clock
  Status = mClock->SetEnable(ClockId, Enable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Enable Clock! Status = %r\n", Status));
  }

  return Status;
}

EFI_STATUS
PowerControl (
  IN UINT32  Index,
  IN BOOLEAN Enable)
{
  EFI_STATUS Status;

  if (Index >= gPlatformInfo.NumberOfHosts) {
    return EFI_INVALID_PARAMETER;
  }

  if ((FixedPcdGet32 (PcdMsdcPreserveBootStateMask) & (1U << Index)) != 0) {
    return EFI_SUCCESS;
  }

  if (mPmic == NULL) {
    return EFI_NOT_READY;
  }

  if (Index == 0 && FixedPcdGetBool(PcdStorageIsEMMC)) {
    // The preceding boot stage may already have powered and trained the
    // soldered eMMC.  Preserve its voltage selection and keep the rail on.
    // Enable VEMC LDO
    Status = mPmic->RegulatorSetEnable("ldo_vemc", Enable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable LDO VEMC! Status = %r\n", Status));
    }
    return Status;
  } else if (Enable) {
    Status = mPmic->RegulatorSetVoltage("ldo_vmch", MSDC_SUPPLY_VOLTAGE_UV);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Set LDO VMCH Voltage! Status = %r\n", Status));
      return Status;
    }

    Status = mPmic->RegulatorSetVoltage("ldo_vmc", MSDC_SUPPLY_VOLTAGE_UV);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Set LDO VMC Voltage! Status = %r\n", Status));
      return Status;
    }

    Status = mPmic->RegulatorSetEnable("ldo_vmch", TRUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable LDO VMCH! Status = %r\n", Status));
      return Status;
    }

    Status = mPmic->RegulatorSetEnable("ldo_vmc", TRUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable LDO VMC! Status = %r\n", Status));
      // Do not leave the card supply enabled after its I/O rail failed.
      mPmic->RegulatorSetEnable("ldo_vmch", FALSE);
      return Status;
    }

    return EFI_SUCCESS;
  } else {
    // Remove the I/O rail first so it cannot back-power a card whose main
    // supply has already been disabled.
    Status = mPmic->RegulatorSetEnable("ldo_vmc", FALSE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Disable LDO VMC! Status = %r\n", Status));
      return Status;
    }

    Status = mPmic->RegulatorSetEnable("ldo_vmch", FALSE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Disable LDO VMCH! Status = %r\n", Status));
    }

    return Status;
  }
}

EFI_STATUS
InitGpio (
  IN UINT32 Index)
{
  STATIC CONST UINT32 EmmcPins[] = {
    58, // DAT0
    59, // DAT1
    60, // DAT2
    61, // DAT3
    62, // DAT4
    63, // DAT5
    64, // DAT6
    65, // DAT7
    56, // CMD
    55  // CLK
  };
  STATIC CONST UINT32 SdCardPins[] = {
    73, // DAT0
    74, // DAT1
    75, // DAT2
    76, // DAT3
    72, // CMD
    71  // CLK
  };
  CONST UINT32 *Pins;
  UINTN         PinCount;
  UINTN         PinIndex;
  EFI_STATUS    Status;

  if (Index >= gPlatformInfo.NumberOfHosts) {
    return EFI_INVALID_PARAMETER;
  }

  if ((FixedPcdGet32 (PcdMsdcPreserveBootStateMask) & (1U << Index)) != 0) {
    return EFI_SUCCESS;
  }

  if (mGpio == NULL) {
    return EFI_NOT_READY;
  }

  if (Index == 0 && FixedPcdGetBool(PcdStorageIsEMMC)) {
    Pins     = EmmcPins;
    PinCount = ARRAY_SIZE (EmmcPins);
  } else {
    Pins     = SdCardPins;
    PinCount = ARRAY_SIZE (SdCardPins);
  }

  for (PinIndex = 0; PinIndex < PinCount; PinIndex++) {
    Status = mGpio->SetMode (Pins[PinIndex], 1);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Configure GPIO %u! Status = %r\n", Pins[PinIndex], Status));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MsdcLibConstructor (VOID)
{
  EFI_STATUS Status;

  // Locate Gpio Protocol
  Status = gBS->LocateProtocol (&gMediaTekGpioProtocolGuid, NULL, (VOID **)&mGpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate GPIO Protocol! Status = %r\n", Status));
    return Status;
  }

  // Locate Clock Protocol
  Status = gBS->LocateProtocol (&gMediaTekClockProtocolGuid, NULL, (VOID **)&mClock);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate Clock Protocol! Status = %r\n", Status));
    return Status;
  }

  // Locate PMIC Protocol
  Status = gBS->LocateProtocol (&gMediaTekPmicProtocolGuid, NULL, (VOID **)&mPmic);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate PMIC Protocol! Status = %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}
