/** @file
  Lenovo Barley SEC initialization.

  LK hands the payload to EL1 with the display pipeline already running.  The
  platform actions required before SEC are disabling the MediaTek watchdog and
  enabling constant blending on LK's stable full-screen layer.  The latter is
  the same MT6768 operation used by the shared platform library and lets a
  standard BGRX GOP ignore the pixel reserved byte.  Display addresses,
  geometry, timings, and ownership otherwise remain exactly as LK configured
  them.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

#define WDT_MODE_KEY             0x22000000U
#define OVL_PITCH_OFFSET(Layer)  (0x44U + (0x20U * (Layer)))
#define OVL_CONST_BLEND          BIT28
#define BARLEY_LK_GOP_LAYER      3U

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

STATIC
VOID
EnableConstantBlending (
  VOID
  )
{
  EFI_MEMORY_REGION_DESCRIPTOR OvlRegion;

  if (EFI_ERROR (LocateMemoryRegionByName ("Display OVL0", &OvlRegion))) {
    return;
  }

  MmioOr32 (
    OvlRegion.Address + OVL_PITCH_OFFSET (BARLEY_LK_GOP_LAYER),
    OVL_CONST_BLEND
    );
}

VOID
PlatformInitialize (
  VOID
  )
{
  DisableWatchDogTimer ();
  EnableConstantBlending ();
}
