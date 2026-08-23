/** @file
  Lenovo Barley SEC initialization.

  LK hands the payload to EL1 with the display pipeline already running.  The
  only platform action required before SEC is disabling the MediaTek watchdog;
  display ownership remains with LK until the passive GOP attaches in DXE.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

#define WDT_MODE_KEY  0x22000000U

STATIC
VOID
DisableWatchDogTimer (
  VOID
  )
{
  EFI_MEMORY_REGION_DESCRIPTOR WatchdogRegion;

  if (EFI_ERROR (LocateMemoryRegionByName ("WatchDog Timer", &WatchdogRegion))) {
    return;
  }

  MmioWrite32 (WatchdogRegion.Address, WDT_MODE_KEY);
}

VOID
PlatformInitialize (
  VOID
  )
{
  DisableWatchDogTimer ();
}
