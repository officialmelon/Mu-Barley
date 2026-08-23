/** @file
  Passive GOP for the scanout surfaces configured by Lenovo LK.

  The driver never accesses display MMIO.  Lenovo's LK log proves that its
  full-screen layer 0 alternates between FB0 and FB2 while its full-screen
  overlay remains on FB1.  Register readback is not a reliable ownership
  contract after LK has handed control to the payload, so every GOP write is
  mirrored to all three buffers in the verified framebuffer carveout.

  PixelBltOnly keeps consumers from mistaking one member of LK's composited
  triple-buffer pool for a single authoritative linear framebuffer.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>

#include <BarleyLkDisplay.h>
#include <Configuration/BootDevices.h>

#define BARLEY_PRIMARY_TARGET       2U

typedef struct {
  EFI_PHYSICAL_ADDRESS    Base;
  UINT32                  Pitch;
  UINTN                   Size;
} BARLEY_SCANOUT_TARGET;

STATIC CONST BARLEY_SCANOUT_TARGET mTargets[] = {
  { BARLEY_FB0_BASE, BARLEY_PITCH_ALIGNED, BARLEY_PITCH_ALIGNED * BARLEY_DISPLAY_HEIGHT },
  { BARLEY_FB1_BASE, BARLEY_PITCH_PACKED,  BARLEY_PITCH_PACKED  * BARLEY_DISPLAY_HEIGHT },
  { BARLEY_FB2_BASE, BARLEY_PITCH_ALIGNED, BARLEY_PITCH_ALIGNED * BARLEY_DISPLAY_HEIGHT }
};

STATIC_ASSERT (
  BARLEY_FB0_BASE + (BARLEY_PITCH_ALIGNED * BARLEY_DISPLAY_HEIGHT) == BARLEY_FB1_BASE,
  "LK FB0 allocation must end at FB1"
  );
STATIC_ASSERT (
  BARLEY_FB2_BASE + (BARLEY_PITCH_ALIGNED * BARLEY_DISPLAY_HEIGHT) <= BARLEY_FB_CARVEOUT_END,
  "LK FB2 allocation must remain inside the framebuffer carveout"
  );

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mModeInfo = {
  0,
  BARLEY_DISPLAY_WIDTH,
  BARLEY_DISPLAY_HEIGHT,
  PixelBltOnly,
  { 0, 0, 0, 0 },
  0
};

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode = {
  1,
  0,
  &mModeInfo,
  sizeof (mModeInfo),
  0,
  0
};

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
VOID
CleanDestination (
  IN CONST BARLEY_SCANOUT_TARGET      *Target,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION Operation,
  IN UINTN                             DestinationX,
  IN UINTN                             DestinationY,
  IN UINTN                             Width,
  IN UINTN                             Height
  )
{
  UINTN Length;

  if (Operation == EfiBltVideoToBltBuffer) {
    return;
  }

  Length = ((Height - 1U) * Target->Pitch) +
           (Width * BARLEY_DISPLAY_BYTES_PER_PIXEL);
  WriteBackDataCacheRange (
    (VOID *)(UINTN)(Target->Base +
                    (DestinationY * Target->Pitch) +
                    (DestinationX * BARLEY_DISPLAY_BYTES_PER_PIXEL)),
    Length
    );
  ArmDataSynchronizationBarrier ();
  ArmInstructionSynchronizationBarrier ();
}

STATIC
BOOLEAN
VideoRectangleValid (
  IN UINTN X,
  IN UINTN Y,
  IN UINTN Width,
  IN UINTN Height
  )
{
  return (X < BARLEY_DISPLAY_WIDTH) &&
         (Y < BARLEY_DISPLAY_HEIGHT) &&
         (Width <= BARLEY_DISPLAY_WIDTH - X) &&
         (Height <= BARLEY_DISPLAY_HEIGHT - Y);
}

STATIC
BOOLEAN
GetBufferStride (
  IN  UINTN X,
  IN  UINTN Y,
  IN  UINTN Width,
  IN  UINTN Height,
  IN  UINTN Delta,
  OUT UINTN *Stride
  )
{
  UINTN EndX;
  UINTN EndY;
  UINTN RowBytes;

  if ((Stride == NULL) ||
      (X > MAX_UINTN - Width) ||
      (Y > MAX_UINTN - Height))
  {
    return FALSE;
  }

  EndX = X + Width;
  EndY = Y + Height;
  if (EndX > (MAX_UINTN / sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))) {
    return FALSE;
  }

  RowBytes = EndX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  *Stride  = (Delta == 0) ? Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL) : Delta;
  if ((*Stride < RowBytes) ||
      ((EndY - 1U) > ((MAX_UINTN - RowBytes) / *Stride)))
  {
    return FALSE;
  }

  return TRUE;
}

STATIC
VOID
FillTarget (
  IN CONST BARLEY_SCANOUT_TARGET         *Target,
  IN CONST EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color,
  IN UINTN                                DestinationX,
  IN UINTN                                DestinationY,
  IN UINTN                                Width,
  IN UINTN                                Height
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL OpaqueColor;
  UINT32                        Pixel;

  OpaqueColor          = *Color;
  OpaqueColor.Reserved = 0xFF;
  CopyMem (&Pixel, &OpaqueColor, sizeof (Pixel));

  for (UINTN Row = 0; Row < Height; Row++) {
    VOID *Destination;

    Destination = (VOID *)(UINTN)(
                    Target->Base +
                    ((DestinationY + Row) * Target->Pitch) +
                    (DestinationX * BARLEY_DISPLAY_BYTES_PER_PIXEL)
                    );
    SetMem32 (Destination, Width * sizeof (Pixel), Pixel);
  }
}

STATIC
VOID
BufferToVideoTarget (
  IN CONST BARLEY_SCANOUT_TARGET     *Target,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *BltBuffer,
  IN UINTN                            SourceX,
  IN UINTN                            SourceY,
  IN UINTN                            DestinationX,
  IN UINTN                            DestinationY,
  IN UINTN                            Width,
  IN UINTN                            Height,
  IN UINTN                            SourceStride
  )
{
  for (UINTN Row = 0; Row < Height; Row++) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *SourceRow;
    volatile UINT32              *DestinationRow;

    SourceRow = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)(
                  (UINT8 *)BltBuffer + ((SourceY + Row) * SourceStride)
                  ) + SourceX;
    DestinationRow = (volatile UINT32 *)(UINTN)(
                       Target->Base +
                       ((DestinationY + Row) * Target->Pitch) +
                       (DestinationX * BARLEY_DISPLAY_BYTES_PER_PIXEL)
                       );

    for (UINTN Column = 0; Column < Width; Column++) {
      EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;
      UINT32                        PackedPixel;

      Pixel          = SourceRow[Column];
      Pixel.Reserved = 0xFF;
      CopyMem (&PackedPixel, &Pixel, sizeof (PackedPixel));
      DestinationRow[Column] = PackedPixel;
    }
  }
}

STATIC
VOID
VideoToBufferTarget (
  IN CONST BARLEY_SCANOUT_TARGET *Target,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
  IN UINTN SourceX,
  IN UINTN SourceY,
  IN UINTN DestinationX,
  IN UINTN DestinationY,
  IN UINTN Width,
  IN UINTN Height,
  IN UINTN DestinationStride
  )
{
  for (UINTN Row = 0; Row < Height; Row++) {
    CONST VOID *Source;
    VOID       *Destination;

    Source = (CONST VOID *)(UINTN)(
               Target->Base +
               ((SourceY + Row) * Target->Pitch) +
               (SourceX * BARLEY_DISPLAY_BYTES_PER_PIXEL)
               );
    Destination = (UINT8 *)BltBuffer +
                  ((DestinationY + Row) * DestinationStride) +
                  (DestinationX * sizeof (*BltBuffer));
    CopyMem (Destination, Source, Width * sizeof (*BltBuffer));
  }
}

STATIC
VOID
VideoToVideoTarget (
  IN CONST BARLEY_SCANOUT_TARGET *Target,
  IN UINTN                        SourceX,
  IN UINTN                        SourceY,
  IN UINTN                        DestinationX,
  IN UINTN                        DestinationY,
  IN UINTN                        Width,
  IN UINTN                        Height,
  IN VOID                        *LineBuffer
  )
{
  BOOLEAN Reverse;

  Reverse = DestinationY > SourceY;
  for (UINTN Index = 0; Index < Height; Index++) {
    UINTN       Row;
    CONST VOID *Source;
    VOID       *Destination;

    Row = Reverse ? (Height - 1U - Index) : Index;
    Source = (CONST VOID *)(UINTN)(
               Target->Base +
               ((SourceY + Row) * Target->Pitch) +
               (SourceX * BARLEY_DISPLAY_BYTES_PER_PIXEL)
               );
    Destination = (VOID *)(UINTN)(
                    Target->Base +
                    ((DestinationY + Row) * Target->Pitch) +
                    (DestinationX * BARLEY_DISPLAY_BYTES_PER_PIXEL)
                    );
    CopyMem (LineBuffer, Source, Width * BARLEY_DISPLAY_BYTES_PER_PIXEL);
    CopyMem (Destination, LineBuffer, Width * BARLEY_DISPLAY_BYTES_PER_PIXEL);
  }
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
  UINTN      BufferStride;
  VOID      *LineBuffer;
  EFI_TPL    OldTpl;
  EFI_STATUS Status;
  UINTN      TargetLimit;

  if ((This == NULL) || (Width == 0) || (Height == 0) ||
      (BltOperation >= EfiGraphicsOutputBltOperationMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  BufferStride = 0;
  LineBuffer   = NULL;

  switch (BltOperation) {
    case EfiBltVideoFill:
      if ((BltBuffer == NULL) ||
          !VideoRectangleValid (
             DestinationX,
             DestinationY,
             Width,
             Height
             ))
      {
        return EFI_INVALID_PARAMETER;
      }

      break;

    case EfiBltBufferToVideo:
      if ((BltBuffer == NULL) ||
          !VideoRectangleValid (
             DestinationX,
             DestinationY,
             Width,
             Height
             ) ||
          !GetBufferStride (
             SourceX,
             SourceY,
             Width,
             Height,
             Delta,
             &BufferStride
             ))
      {
        return EFI_INVALID_PARAMETER;
      }

      break;

    case EfiBltVideoToBltBuffer:
      if ((BltBuffer == NULL) ||
          !VideoRectangleValid (SourceX, SourceY, Width, Height) ||
          !GetBufferStride (
             DestinationX,
             DestinationY,
             Width,
             Height,
             Delta,
             &BufferStride
             ))
      {
        return EFI_INVALID_PARAMETER;
      }

      break;

    case EfiBltVideoToVideo:
      if (!VideoRectangleValid (SourceX, SourceY, Width, Height) ||
          !VideoRectangleValid (
             DestinationX,
             DestinationY,
             Width,
             Height
             ))
      {
        return EFI_INVALID_PARAMETER;
      }

      LineBuffer = AllocatePool (Width * BARLEY_DISPLAY_BYTES_PER_PIXEL);
      if (LineBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      break;

    default:
      return EFI_INVALID_PARAMETER;
  }

  OldTpl      = gBS->RaiseTPL (TPL_NOTIFY);
  Status      = EFI_SUCCESS;
  TargetLimit = (BltOperation == EfiBltVideoToBltBuffer) ? 1U : ARRAY_SIZE (mTargets);

  for (UINTN Slot = 0; Slot < TargetLimit; Slot++) {
    UINTN Index;

    Index = (BltOperation == EfiBltVideoToBltBuffer) ? BARLEY_PRIMARY_TARGET : Slot;
    switch (BltOperation) {
      case EfiBltVideoFill:
        FillTarget (
          &mTargets[Index],
          BltBuffer,
          DestinationX,
          DestinationY,
          Width,
          Height
          );
        break;

      case EfiBltBufferToVideo:
        BufferToVideoTarget (
          &mTargets[Index],
          BltBuffer,
          SourceX,
          SourceY,
          DestinationX,
          DestinationY,
          Width,
          Height,
          BufferStride
          );
        break;

      case EfiBltVideoToBltBuffer:
        VideoToBufferTarget (
          &mTargets[Index],
          BltBuffer,
          SourceX,
          SourceY,
          DestinationX,
          DestinationY,
          Width,
          Height,
          BufferStride
          );
        break;

      case EfiBltVideoToVideo:
        VideoToVideoTarget (
          &mTargets[Index],
          SourceX,
          SourceY,
          DestinationX,
          DestinationY,
          Width,
          Height,
          LineBuffer
          );
        break;

      default:
        Status = EFI_INVALID_PARAMETER;
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
  if (LineBuffer != NULL) {
    FreePool (LineBuffer);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
BarleySetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN UINT32                        ModeNumber
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black;

  if ((This == NULL) || (ModeNumber != 0)) {
    return EFI_UNSUPPORTED;
  }

  ZeroMem (&Black, sizeof (Black));
  Black.Reserved   = 0xFF;
  This->Mode->Mode = 0;
  return This->Blt (
                 This,
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
  EFI_STATUS Status;

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

  return mGop.SetMode (&mGop, 0);
}
