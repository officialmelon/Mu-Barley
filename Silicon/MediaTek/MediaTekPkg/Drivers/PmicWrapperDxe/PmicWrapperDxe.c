#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>

#include <Library/PmicWrapperImplLib.h>

#include <Protocol/MtkPmicWrapper.h>

typedef enum {
  WacsFsmIdle = 0,
  WacsFsmWfVldClr = 6,
} WACS_FSM_STATE;

#define PWRAP_POLL_DELAY_US    10
#define PWRAP_POLL_TIMEOUT_US  10000

STATIC EFI_MEMORY_REGION_DESCRIPTOR mPmicWrapperRegion;

STATIC
UINT32
PmicWrapperRead(
  IN  UINT16 Reg)
{
  return MmioRead32 (mPmicWrapperRegion.Address + gPlatformInfo.RegMap[Reg]);
}

STATIC
VOID
PmicWrapperWrite(
  IN  UINT16 Reg,
  IN  UINT32 Value)
{
  MmioWrite32 (mPmicWrapperRegion.Address + gPlatformInfo.RegMap[Reg], Value);
}

STATIC
WACS_FSM_STATE
WacsGetFsm ()
{
  UINT32 Value;

  // Read Data
  Value = PmicWrapperRead (PmicWrapperWacs2RData);

  // Get FSM State
  if (gPlatformInfo.ArbCapabilities) {
    Value = (Value >> 1) & 0x7;
  } else {
    Value = (Value >> 16) & 0x7;
  }

  return Value;
}

STATIC
EFI_STATUS
WacsWaitFor (
  IN WACS_FSM_STATE Fsm)
{
  UINTN Elapsed;

  // PMIC wrapper transactions normally complete in a few microseconds.  Do
  // not let a broken or unavailable wrapper stall the DXE dispatcher forever.
  for (Elapsed = 0; Elapsed < PWRAP_POLL_TIMEOUT_US; Elapsed += PWRAP_POLL_DELAY_US) {
    if (WacsGetFsm () == Fsm) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (PWRAP_POLL_DELAY_US);
  }

  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
WacsCommand (
  IN UINT32  Address,
  IN UINT32  Data,
  IN BOOLEAN IsWrite)
{
  EFI_STATUS Status;
  UINT32 WacsCommand;

  // A timed-out read may leave the wrapper waiting for VLDCLR.  Clear that
  // stale result before accepting a new transaction.
  if (WacsGetFsm () == WacsFsmWfVldClr) {
    PmicWrapperWrite (PmicWrapperWacs2VldClr, 1);
  }

  // Wait until FSM reaches idle state
  Status = WacsWaitFor (WacsFsmIdle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Encode Address, data and write mode
  if (gPlatformInfo.ArbCapabilities) {
    WacsCommand = Address;
    if (IsWrite) {
      PmicWrapperWrite (PmicWrapperSwinf2WData31, Data);
      WacsCommand |= (1 << 29);
    }
  } else {
    WacsCommand = (IsWrite << 31) | ((Address >> 1) << 16) | Data;
  }

  // Write Command
  PmicWrapperWrite (PmicWrapperWacs2Cmd, WacsCommand);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PmicWrapperImplRead (
  IN  UINT16  Address,
  OUT UINT16 *Value)
{
  EFI_STATUS Status;
  UINT32 Result;

  if (Value == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Send read transaction and wait until result become valid
  Status = WacsCommand (Address, 0, FALSE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = WacsWaitFor (WacsFsmWfVldClr);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Read data
  Result = PmicWrapperRead (gPlatformInfo.ArbCapabilities ? PmicWrapperSwinf2RData31 : PmicWrapperWacs2RData);
  PmicWrapperWrite (PmicWrapperWacs2VldClr, 1);

  *Value = (Result & 0xFFFF);

  return WacsWaitFor (WacsFsmIdle);
}

STATIC
EFI_STATUS
PmicWrapperImplWrite (
  IN  UINT16 Address,
  IN  UINT16 Value)
{
  EFI_STATUS Status;

  // Send write transaction
  Status = WacsCommand (Address, Value, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Ensure the write completed before reporting success to a read-modify-write
  // caller or starting a subsequent transaction.
  return WacsWaitFor (WacsFsmIdle);
}

STATIC MTK_PMIC_WRAPPER_PROTOCOL mPmicWrapper = {
  PmicWrapperImplRead,
  PmicWrapperImplWrite
};

EFI_STATUS
EFIAPI
InitPmicWrapper (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  UINT32     InitState;

  // Locate PMIC Wrapper Memory Region
  Status = LocateMemoryRegionByName ("PMIC Wrapper", &mPmicWrapperRegion);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate PMIC Wrapper Memory Region! Status = %r\n", Status));
    return Status;
  }

  // Read init state of PMIC Wrapper
  InitState = PmicWrapperRead (PmicWrapperInitDone2);
  if (InitState != 1) {
    DEBUG ((EFI_D_ERROR, "PMIC Wrapper Not Initialized! INIT_DONE2 = 0x%x\n", InitState));
    return EFI_NOT_READY;
  }

  // Register PMIC Wrapper Protocol
  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle, &gMediaTekPmicWrapperProtocolGuid, &mPmicWrapper, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Register PMIC Wrapper Protocol! Status = %r\n", Status));
    return Status;
  }

  return Status;
}
