/**
  Copyright (c) 2011-2012, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

#include "PlatformRegisters.h"

#if BARLEY_STAGE_TRACE == 1
VOID
EFIAPI
BarleyEarlyVisualTrace (
  IN UINT32 Stage,
  IN UINT32 PatternA,
  IN UINT32 PatternB
  );
#endif

VOID
DisableWatchDogTimer ()
{
  EFI_STATUS                   Status;
  EFI_MEMORY_REGION_DESCRIPTOR WDogRegion;

  // Locate "WatchDog Timer" Memory Region
  Status = LocateMemoryRegionByName ("WatchDog Timer", &WDogRegion);
  if (EFI_ERROR (Status)) {
    return;
  }

  // Disable WatchDog Timer
  MmioWrite32 (WDogRegion.Address, WDT_MODE_KEY);
}

VOID
EnableConstantBlending ()
{
  EFI_STATUS                   Status;
  EFI_MEMORY_REGION_DESCRIPTOR OvlRegion;

  // Locate "Display OVL" Memory Region
  Status = LocateMemoryRegionByName ("Display OVL", &OvlRegion);
  if (EFI_ERROR (Status)) {
    return;
  }

  // Enable Constant Blending
  MmioOr32 (OvlRegion.Address + OVL_PITCH_OFFSET (0), OVL_CONST_BLEND);
}

VOID
PlatformInitialize ()
{
#if BARLEY_STAGE_TRACE == 1
  // First C-level platform marker; watchdog/display initialization follows.
  BarleyEarlyVisualTrace (0x06, 0x0000FFFF, 0x0000FFFF);
#endif

  // Disable WatchDog Timer
  DisableWatchDogTimer ();

  // Enable Constant Blending
  EnableConstantBlending ();
}
