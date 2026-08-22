#include <Uefi.h>

#include <BarleyEarlyVisualTrace.h>

EFI_STATUS
EFIAPI
EarlyVisualTraceDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  // Do not consume either argument or call any UEFI service. If this executes,
  // the DXE core dispatcher has been reached and loaded its first trace driver.
  BarleyEarlyVisualTrace (
    BARLEY_TRACE_STAGE_DXE_CORE_REACHED,
    BARLEY_TRACE_DXE_CORE_REACHED_A,
    BARLEY_TRACE_DXE_CORE_REACHED_B
    );
  return EFI_SUCCESS;
}
