#include <Uefi.h>

VOID
EFIAPI
BarleyEarlyVisualTrace (
  IN UINT32 Stage,
  IN UINT32 PatternA,
  IN UINT32 PatternB
  );

RETURN_STATUS
EFIAPI
BarleyShellTraceLibConstructor (
  VOID
  )
{
  /* UEFI application constructors run immediately before Shell UefiMain. */
  BarleyEarlyVisualTrace (15, 0, 0);
  return RETURN_SUCCESS;
}
