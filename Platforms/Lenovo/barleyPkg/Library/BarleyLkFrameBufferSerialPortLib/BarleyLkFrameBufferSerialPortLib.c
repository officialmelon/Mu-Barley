/** @file
  DEBUG SerialPortLib backed by the scanout surfaces inherited from Lenovo LK.

  No display registers are accessed.  Every character is mirrored to all three
  verified LK surfaces because LK can scan out either aligned surface while a
  packed full-screen overlay remains active.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/PcdLib.h>
#include <Library/SerialPortLib.h>
#include <Library/TimerLib.h>

#include <BarleyLkDisplay.h>

// Reuse the same compact 5x12 font as the generic Silicium framebuffer logger.
#include "../../../../../Silicon/Silicium/SiliciumPkg/Library/FrameBufferSerialPortLib/Font.h"

#define BARLEY_BLACK_PIXEL       0xFF000000U
#define BARLEY_WHITE_PIXEL       0xFFFFFFFFU

typedef struct {
  EFI_PHYSICAL_ADDRESS  Base;
  UINT32                Pitch;
} BARLEY_DEBUG_TARGET;

typedef struct {
  UINT32  Signature;
  UINT32  Column;
  UINT32  Row;
} BARLEY_DEBUG_CURSOR;

#define BARLEY_DEBUG_CURSOR_SIGNATURE  SIGNATURE_32 ('B', 'D', 'B', 'G')

STATIC CONST BARLEY_DEBUG_TARGET  mTargets[] = {
  { BARLEY_FB0_BASE, BARLEY_PITCH_ALIGNED },
  { BARLEY_FB1_BASE, BARLEY_PITCH_PACKED  },
  { BARLEY_FB2_BASE, BARLEY_PITCH_ALIGNED }
};

#define BARLEY_CURSOR_ADDRESS  \
  (BARLEY_FB_CARVEOUT_END - sizeof (BARLEY_DEBUG_CURSOR))

STATIC_ASSERT (
  BARLEY_DISPLAY_WIDTH * BARLEY_DISPLAY_BYTES_PER_PIXEL == BARLEY_PITCH_PACKED,
  "Barley packed pitch must match the visible row size"
  );
STATIC_ASSERT (
  BARLEY_FB0_BASE + (BARLEY_PITCH_ALIGNED * BARLEY_DISPLAY_HEIGHT) == BARLEY_FB1_BASE,
  "LK FB0 allocation must end at FB1"
  );
STATIC_ASSERT (
  BARLEY_FB2_BASE + (BARLEY_PITCH_ALIGNED * BARLEY_DISPLAY_HEIGHT) < BARLEY_CURSOR_ADDRESS,
  "Debug cursor must remain beyond every LK scanout surface"
  );

STATIC
UINT8
GetFontScale (
  VOID
  )
{
  UINT32  ShorterDimension;

  ShorterDimension = MIN (BARLEY_DISPLAY_WIDTH, BARLEY_DISPLAY_HEIGHT);
  return (ShorterDimension < 426U) ? 1U : (UINT8)(ShorterDimension / 426U);
}

STATIC
VOID
GetTextDimensions (
  IN  UINT8   FontScale,
  OUT UINT32  *CellWidth,
  OUT UINT32  *CellHeight,
  OUT UINT32  *Columns,
  OUT UINT32  *Rows
  )
{
  *CellWidth  = (FONT_WIDTH + 1U) * FontScale;
  *CellHeight = (FONT_HEIGHT - 4U) * FontScale;
  *Columns    = BARLEY_DISPLAY_WIDTH / *CellWidth;
  *Rows       = BARLEY_DISPLAY_HEIGHT / *CellHeight;
}

STATIC
BARLEY_DEBUG_CURSOR *
GetCursor (
  VOID
  )
{
  return (BARLEY_DEBUG_CURSOR *)(UINTN)BARLEY_CURSOR_ADDRESS;
}

STATIC
VOID
CleanCursor (
  VOID
  )
{
  WriteBackDataCacheRange (
    (VOID *)(UINTN)BARLEY_CURSOR_ADDRESS,
    sizeof (BARLEY_DEBUG_CURSOR)
    );
  ArmDataSynchronizationBarrier ();
}

STATIC
VOID
CleanRectangle (
  IN CONST BARLEY_DEBUG_TARGET  *Target,
  IN UINT32                     X,
  IN UINT32                     Y,
  IN UINT32                     Width,
  IN UINT32                     Height
  )
{
  UINTN  Length;

  Length = ((Height - 1U) * Target->Pitch) +
           (Width * BARLEY_DISPLAY_BYTES_PER_PIXEL);
  WriteBackDataCacheRange (
    (VOID *)(UINTN)(Target->Base +
                    (Y * Target->Pitch) +
                    (X * BARLEY_DISPLAY_BYTES_PER_PIXEL)),
    Length
    );
}

STATIC
VOID
DrawGlyphToTarget (
  IN CONST BARLEY_DEBUG_TARGET  *Target,
  IN UINT32                     X,
  IN UINT32                     Y,
  IN UINT64                     Glyph,
  IN UINT8                      FontScale
  )
{
  UINT32  BottomGlyph;
  UINT32  CellHeight;
  UINT32  CellWidth;
  UINT32  Columns;
  UINT32  RowData;
  UINT32  Rows;
  UINT32  TopGlyph;

  TopGlyph    = (UINT32)(Glyph >> 32);
  BottomGlyph = (UINT32)Glyph;
  GetTextDimensions (FontScale, &CellWidth, &CellHeight, &Columns, &Rows);

  RowData = TopGlyph;
  for (UINT32 GlyphRow = 0; GlyphRow < (FONT_HEIGHT - 4U); GlyphRow++) {
    if (GlyphRow == 6U) {
      RowData = BottomGlyph;
    }

    for (UINT32 ScaleY = 0; ScaleY < FontScale; ScaleY++) {
      volatile UINT32  *Pixels;

      Pixels = (volatile UINT32 *)(UINTN)(
                 Target->Base +
                 ((Y + (GlyphRow * FontScale) + ScaleY) * Target->Pitch) +
                 (X * BARLEY_DISPLAY_BYTES_PER_PIXEL)
                 );

      for (UINT32 GlyphColumn = 0; GlyphColumn < (FONT_WIDTH + 1U); GlyphColumn++) {
        UINT32  Color;

        Color = ((GlyphColumn < FONT_WIDTH) &&
                 ((RowData & (1U << GlyphColumn)) != 0)) ?
                BARLEY_WHITE_PIXEL : BARLEY_BLACK_PIXEL;
        for (UINT32 ScaleX = 0; ScaleX < FontScale; ScaleX++) {
          *Pixels++ = Color;
        }
      }
    }

    RowData >>= FONT_WIDTH;
  }

  CleanRectangle (Target, X, Y, CellWidth, CellHeight);
}

STATIC
VOID
DrawGlyph (
  IN UINT32  X,
  IN UINT32  Y,
  IN UINT64  Glyph,
  IN UINT8   FontScale
  )
{
  for (UINTN Index = 0; Index < ARRAY_SIZE (mTargets); Index++) {
    DrawGlyphToTarget (&mTargets[Index], X, Y, Glyph, FontScale);
  }

  ArmDataSynchronizationBarrier ();
}

STATIC
VOID
ClearTargets (
  VOID
  )
{
  for (UINTN Index = 0; Index < ARRAY_SIZE (mTargets); Index++) {
    CONST BARLEY_DEBUG_TARGET  *Target;

    Target = &mTargets[Index];
    for (UINT32 Row = 0; Row < BARLEY_DISPLAY_HEIGHT; Row++) {
      SetMem32 (
        (VOID *)(UINTN)(Target->Base + (Row * Target->Pitch)),
        BARLEY_DISPLAY_WIDTH * BARLEY_DISPLAY_BYTES_PER_PIXEL,
        BARLEY_BLACK_PIXEL
        );
    }

    CleanRectangle (
      Target,
      0,
      0,
      BARLEY_DISPLAY_WIDTH,
      BARLEY_DISPLAY_HEIGHT
      );
  }

  ArmDataSynchronizationBarrier ();
}

STATIC
VOID
AdvanceNewLine (
  IN OUT BARLEY_DEBUG_CURSOR  *Cursor,
  IN     UINT32               Rows
  )
{
  MicroSecondDelay (FixedPcdGet32 (PcdFrameBufferDelay));

  Cursor->Column = 0;
  Cursor->Row++;
  if (Cursor->Row >= Rows) {
    ClearTargets ();
    Cursor->Row = 0;
  }
}

STATIC
VOID
WriteCharacter (
  IN OUT BARLEY_DEBUG_CURSOR  *Cursor,
  IN     UINT8                Character,
  IN     UINT8                FontScale,
  IN     UINT32               CellWidth,
  IN     UINT32               CellHeight,
  IN     UINT32               Columns,
  IN     UINT32               Rows
  )
{
  STATIC CONST UINT64  GlyphFont[] = GLYPH_FONT;

  if (Character >= 127U) {
    return;
  }

  if (Character < 32U) {
    if (Character == '\n') {
      AdvanceNewLine (Cursor, Rows);
    } else if (Character == '\r') {
      Cursor->Column = 0;
    }

    return;
  }

  if ((Cursor->Column == 0U) && (Character == ' ')) {
    return;
  }

  DrawGlyph (
    Cursor->Column * CellWidth,
    Cursor->Row * CellHeight,
    GlyphFont[Character - 32U],
    FontScale
    );

  Cursor->Column++;
  if (Cursor->Column >= Columns) {
    AdvanceNewLine (Cursor, Rows);
  }
}

RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  return RETURN_SUCCESS;
}

UINTN
EFIAPI
SerialPortWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  BARLEY_DEBUG_CURSOR  *Cursor;
  UINT32               CellHeight;
  UINT32               CellWidth;
  UINT32               Columns;
  UINT8                FontScale;
  BOOLEAN              InterruptState;
  UINT32               Rows;

  if ((Buffer == NULL) || (NumberOfBytes == 0U)) {
    return 0;
  }

  FontScale = GetFontScale ();
  GetTextDimensions (FontScale, &CellWidth, &CellHeight, &Columns, &Rows);
  Cursor = GetCursor ();

  InterruptState = ArmGetInterruptState ();
  if (InterruptState) {
    ArmDisableInterrupts ();
  }

  if ((Cursor->Signature != BARLEY_DEBUG_CURSOR_SIGNATURE) ||
      (Cursor->Column >= Columns) ||
      (Cursor->Row >= Rows))
  {
    Cursor->Signature = BARLEY_DEBUG_CURSOR_SIGNATURE;
    Cursor->Column = 0;
    Cursor->Row    = 0;
  }

  for (UINTN Index = 0; Index < NumberOfBytes; Index++) {
    WriteCharacter (
      Cursor,
      Buffer[Index],
      FontScale,
      CellWidth,
      CellHeight,
      Columns,
      Rows
      );
  }

  CleanCursor ();
  if (InterruptState) {
    ArmEnableInterrupts ();
  }

  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortRead (
  OUT UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  return 0;
}

BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  return FALSE;
}

RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32  Control
  )
{
  return RETURN_UNSUPPORTED;
}

RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32  *Control
  )
{
  return RETURN_UNSUPPORTED;
}

RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  return RETURN_UNSUPPORTED;
}
