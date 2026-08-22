/**
  Copyright (c) 2011-2012, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

#include "PlatformRegisters.h"

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

#if BARLEY_STAGE_TRACE != 1
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
#endif

VOID
PlatformInitialize ()
{
  // Disable WatchDog Timer
  DisableWatchDogTimer ();

  // M2.13 treats LK's display pipeline as immutable. Lancelot normally sets
  // one OVL pitch bit here; the diagnostic build must perform no display MMIO.
#if BARLEY_STAGE_TRACE != 1
  EnableConstantBlending ();
#endif
}
