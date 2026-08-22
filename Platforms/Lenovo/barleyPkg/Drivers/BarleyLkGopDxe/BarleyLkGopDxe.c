#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/ArmLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/FrameBufferBltLib.h>
#include <BarleyEarlyVisualTrace.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>

#include <BarleyLkDisplay.h>
#include <Configuration/BootDevices.h>

#define BARLEY_MAX_SCANOUT_TARGETS  10U
#define BARLEY_MIN_SCANOUT_ADDRESS  0x70000000ULL
#define BARLEY_MAX_SCANOUT_ADDRESS  0x80000000ULL
#define BARLEY_MIN_PITCH            (BARLEY_DISPLAY_WIDTH * BARLEY_DISPLAY_BYTES_PER_PIXEL)
#define BARLEY_MAX_PITCH            0x00002000U

typedef struct {
  EFI_PHYSICAL_ADDRESS    Base;
  UINT32                  Pitch;
  UINT32                  PixelsPerScanLine;
  UINTN                   Size;
  FRAME_BUFFER_CONFIGURE *Configuration;
} BARLEY_SCANOUT_TARGET;

STATIC BARLEY_SCANOUT_TARGET mTargets[BARLEY_MAX_SCANOUT_TARGETS];
STATIC UINTN                 mTargetCount;

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mModeInfo = {
  0,
  BARLEY_DISPLAY_WIDTH,
  BARLEY_DISPLAY_HEIGHT,
  PixelBlueGreenRedReserved8BitPerColor,
  { 0, 0, 0, 0 },
  BARLEY_DISPLAY_WIDTH
};

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode = {
  1,
  0,
  &mModeInfo,
  sizeof (mModeInfo),
  BARLEY_LK_LOGO_BASE,
  BARLEY_LK_LOGO_SIZE
};

STATIC
BOOLEAN
ValidTarget (
  IN EFI_PHYSICAL_ADDRESS Base,
  IN UINT32               Pitch
  )
{
  UINT64 Size;

  if ((Base < BARLEY_MIN_SCANOUT_ADDRESS) ||
      (Pitch < BARLEY_MIN_PITCH) ||
      (Pitch > BARLEY_MAX_PITCH) ||
      ((Pitch & (BARLEY_DISPLAY_BYTES_PER_PIXEL - 1U)) != 0))
  {
    return FALSE;
  }

  Size = MultU64x32 (Pitch, BARLEY_DISPLAY_HEIGHT);
  return (Base + Size > Base) && (Base + Size <= BARLEY_MAX_SCANOUT_ADDRESS);
}

STATIC
VOID
AddTarget (
  IN EFI_PHYSICAL_ADDRESS Base,
  IN UINT32               Pitch
  )
{
  BARLEY_SCANOUT_TARGET *Target;

  if (!ValidTarget (Base, Pitch)) {
    return;
  }

  for (UINTN Index = 0; Index < mTargetCount; Index++) {
    if (mTargets[Index].Base == Base) {
      return;
    }
  }

  if (mTargetCount == ARRAY_SIZE (mTargets)) {
    return;
  }

  Target                    = &mTargets[mTargetCount++];
  Target->Base              = Base;
  Target->Pitch             = Pitch;
  Target->PixelsPerScanLine = Pitch / BARLEY_DISPLAY_BYTES_PER_PIXEL;
  Target->Size              = Pitch * BARLEY_DISPLAY_HEIGHT;
}

STATIC
VOID
DiscoverOvlTargets (
  IN UINTN  Base,
  IN UINT32 LayerCount
  )
{
  UINT32 Enabled;
  UINT32 Source;

  Enabled = MmioRead32 (Base + BARLEY_OVL_EN);
  Source  = MmioRead32 (Base + BARLEY_OVL_SRC_CON);
  if ((Enabled & BIT0) == 0) {
    return;
  }

  for (UINT32 Layer = 0; Layer < LayerCount; Layer++) {
    EFI_PHYSICAL_ADDRESS Address;
    UINT32               Pitch;
    UINT32               Size;
    UINT32               Width;
    UINT32               Height;

    if ((Source & (BIT0 << Layer)) == 0) {
      continue;
    }

    Address = MmioRead32 (Base + BARLEY_OVL_L_ADDR (Layer));
    Pitch   = MmioRead32 (Base + BARLEY_OVL_L_PITCH (Layer)) & 0xFFFFU;
    Size    = MmioRead32 (Base + BARLEY_OVL_L_SRC_SIZE (Layer));
    Width   = Size & 0x1FFFU;
    Height  = (Size >> 16) & 0x1FFFU;

    // LK logs prove 1200x1920. Accept zero geometry only for diagnostic mirroring.
    if (((Width == BARLEY_DISPLAY_WIDTH) && (Height == BARLEY_DISPLAY_HEIGHT)) ||
        ((Width == 0) && (Height == 0)))
    {
      AddTarget (Address, Pitch);
    }
  }
}

STATIC
VOID
DiscoverLiveTargets (
  VOID
  )
{
  UINT32 RdmaGlobal;

  ZeroMem (mTargets, sizeof (mTargets));
  mTargetCount = 0;

  // Prefer live OVL sources so GOP FrameBufferBase describes the active path.
  DiscoverOvlTargets (BARLEY_OVL0_BASE, 4);
  DiscoverOvlTargets (BARLEY_OVL0_2L_BASE, 2);

  RdmaGlobal = MmioRead32 (BARLEY_RDMA0_BASE + BARLEY_RDMA_GLOBAL_CON);
  if ((RdmaGlobal & BARLEY_RDMA_MEMORY_MODE) != 0) {
    AddTarget (
      MmioRead32 (BARLEY_RDMA0_BASE + BARLEY_RDMA_MEM_START_ADDR),
      MmioRead32 (BARLEY_RDMA0_BASE + BARLEY_RDMA_MEM_SRC_PITCH) & 0xFFFFU
      );
  }

  // Retain the two requested diagnostic mirrors even when neither is scanout.
  AddTarget (BARLEY_LK_LOGO_BASE, BARLEY_MIN_PITCH);
  AddTarget (BARLEY_FDT_DISPLAY_BASE, BARLEY_MIN_PITCH);
}

STATIC
EFI_STATUS
ConfigureTargets (
  VOID
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION TargetInfo;

  for (UINTN Index = 0; Index < mTargetCount; Index++) {
    EFI_STATUS Status;
    UINTN      ConfigurationSize;

    CopyMem (&TargetInfo, &mModeInfo, sizeof (TargetInfo));
    TargetInfo.PixelsPerScanLine = mTargets[Index].PixelsPerScanLine;
    ConfigurationSize            = 0;
    Status = FrameBufferBltConfigure (
               (VOID *)(UINTN)mTargets[Index].Base,
               &TargetInfo,
               NULL,
               &ConfigurationSize
               );
    if (Status != RETURN_BUFFER_TOO_SMALL) {
      return Status;
    }

    mTargets[Index].Configuration = AllocatePool (ConfigurationSize);
    if (mTargets[Index].Configuration == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Status = FrameBufferBltConfigure (
               (VOID *)(UINTN)mTargets[Index].Base,
               &TargetInfo,
               mTargets[Index].Configuration,
               &ConfigurationSize
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (mTargetCount == 0) {
    return EFI_NOT_FOUND;
  }

  mModeInfo.PixelsPerScanLine = mTargets[0].PixelsPerScanLine;
  mMode.FrameBufferBase       = mTargets[0].Base;
  mMode.FrameBufferSize       = mTargets[0].Size;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
BarleyQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                 ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
  )
{
  if ((This == NULL) || (SizeOfInfo == NULL) || (Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (ModeNumber != 0) {
    return EFI_UNSUPPORTED;
  }

  *Info = AllocateCopyPool (sizeof (mModeInfo), &mModeInfo);
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (mModeInfo);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
BarleySetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN UINT32                        ModeNumber
  )
{
  if ((This == NULL) || (ModeNumber != 0)) {
    return EFI_UNSUPPORTED;
  }

  This->Mode->Mode = 0;
  return EFI_SUCCESS;
}

STATIC
VOID
CleanDestination (
  IN CONST BARLEY_SCANOUT_TARGET          *Target,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION     Operation,
  IN UINTN                                 DestinationX,
  IN UINTN                                 DestinationY,
  IN UINTN                                 Width,
  IN UINTN                                 Height
  )
{
  UINTN StartY;
  UINTN Length;

  if (Operation == EfiBltVideoToBltBuffer) {
    return;
  }

  StartY = DestinationY;
  Length = ((Height - 1U) * Target->Pitch) + (Width * BARLEY_DISPLAY_BYTES_PER_PIXEL);
  WriteBackDataCacheRange (
    (VOID *)(UINTN)(Target->Base + (StartY * Target->Pitch) +
                    (DestinationX * BARLEY_DISPLAY_BYTES_PER_PIXEL)),
    Length
    );
  ArmDataSynchronizationBarrier ();
  ArmInstructionSynchronizationBarrier ();
}

STATIC
EFI_STATUS
EFIAPI
BarleyBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL      *This,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     *BltBuffer OPTIONAL,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN UINTN                              SourceX,
  IN UINTN                              SourceY,
  IN UINTN                              DestinationX,
  IN UINTN                              DestinationY,
  IN UINTN                              Width,
  IN UINTN                              Height,
  IN UINTN                              Delta OPTIONAL
  )
{
  EFI_STATUS Status;
  EFI_TPL    OldTpl;
  UINTN      Limit;

  if ((This == NULL) || (Width == 0) || (Height == 0) ||
      (BltOperation >= EfiGraphicsOutputBltOperationMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  Status = EFI_SUCCESS;
  Limit  = (BltOperation == EfiBltVideoToBltBuffer) ? 1U : mTargetCount;

  for (UINTN Index = 0; Index < Limit; Index++) {
    Status = FrameBufferBlt (
               mTargets[Index].Configuration,
               BltBuffer,
               BltOperation,
               SourceX,
               SourceY,
               DestinationX,
               DestinationY,
               Width,
               Height,
               Delta
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    CleanDestination (
      &mTargets[Index],
      BltOperation,
      DestinationX,
      DestinationY,
      Width,
      Height
      );
  }

  gBS->RestoreTPL (OldTpl);
  return Status;
}

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop = {
  BarleyQueryMode,
  BarleySetMode,
  BarleyBlt,
  &mMode
};

EFI_STATUS
EFIAPI
BarleyLkGopDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL White;
  EFI_STATUS                    Status;

  BarleyEarlyVisualTrace (BARLEY_TRACE_STAGE_GOP_ENTRY, 0, 0);

  DiscoverLiveTargets ();
  Status = ConfigureTargets ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDevicePathProtocolGuid,
                  &DisplayDevicePath,
                  &gEfiGraphicsOutputProtocolGuid,
                  &mGop,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ZeroMem (&Black, sizeof (Black));
  SetMem (&White, sizeof (White), 0xFF);
  Status = mGop.Blt (
                  &mGop,
                  &Black,
                  EfiBltVideoFill,
                  0,
                  0,
                  0,
                  0,
                  BARLEY_DISPLAY_WIDTH,
                  BARLEY_DISPLAY_HEIGHT,
                  0
                  );
  if (!EFI_ERROR (Status)) {
    Status = mGop.Blt (
                    &mGop,
                    &White,
                    EfiBltVideoFill,
                    0,
                    0,
                    100,
                    300,
                    1000,
                    1320,
                    0
                    );
  }

  return Status;
}
