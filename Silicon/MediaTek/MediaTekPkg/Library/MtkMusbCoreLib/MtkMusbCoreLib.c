/** @file
  MediaTek MUSB core access library.

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>

#include <Library/MtkMusbCoreLib.h>

UINT8
MtkMusbRead8 (
  UINTN  Base,
  UINTN  Offset
  )
{
  return MmioRead8 (Base + Offset);
}

UINT16
MtkMusbRead16 (
  UINTN  Base,
  UINTN  Offset
  )
{
  return MmioRead16 (Base + Offset);
}

UINT32
MtkMusbRead32 (
  UINTN  Base,
  UINTN  Offset
  )
{
  return MmioRead32 (Base + Offset);
}

VOID
MtkMusbWrite8 (
  UINTN  Base,
  UINTN  Offset,
  UINT8  Value
  )
{
  MmioWrite8 (Base + Offset, Value);
}

VOID
MtkMusbWrite16 (
  UINTN  Base,
  UINTN  Offset,
  UINT16 Value
  )
{
  MmioWrite16 (Base + Offset, Value);
}

VOID
MtkMusbWrite32 (
  UINTN  Base,
  UINTN  Offset,
  UINT32 Value
  )
{
  MmioWrite32 (Base + Offset, Value);
}

VOID
MtkMusbSelectEndpoint (
  UINTN  Base,
  UINT8  EpNum
  )
{
  MmioWrite8 (Base + MUSB_INDEX, EpNum & (MTK_MUSB_NUM_EPS - 1));
}

UINT8
MtkMusbReadIndexed8 (
  UINTN  Base,
  UINT8  EpNum,
  UINTN  Offset
  )
{
  MtkMusbSelectEndpoint (Base, EpNum);
  return MmioRead8 (Base + MUSB_INDEXED_OFFSET (EpNum, Offset));
}

UINT16
MtkMusbReadIndexed16 (
  UINTN  Base,
  UINT8  EpNum,
  UINTN  Offset
  )
{
  MtkMusbSelectEndpoint (Base, EpNum);
  return MmioRead16 (Base + MUSB_INDEXED_OFFSET (EpNum, Offset));
}

VOID
MtkMusbWriteIndexed8 (
  UINTN  Base,
  UINT8  EpNum,
  UINTN  Offset,
  UINT8  Value
  )
{
  MtkMusbSelectEndpoint (Base, EpNum);
  MmioWrite8 (Base + MUSB_INDEXED_OFFSET (EpNum, Offset), Value);
}

VOID
MtkMusbWriteIndexed16 (
  UINTN  Base,
  UINT8  EpNum,
  UINTN  Offset,
  UINT16 Value
  )
{
  MtkMusbSelectEndpoint (Base, EpNum);
  MmioWrite16 (Base + MUSB_INDEXED_OFFSET (EpNum, Offset), Value);
}

UINT8
MtkMusbClearByte (
  UINTN  Base,
  UINTN  Offset
  )
{
  UINT8 Data;

  /* W1C: read, write back.  U-Boot glue mtk_musb_clearb(). */
  Data = MmioRead8 (Base + Offset);
  MmioWrite8 (Base + Offset, Data);
  return Data;
}

UINT16
MtkMusbClearWord (
  UINTN  Base,
  UINTN  Offset
  )
{
  UINT16 Data;

  Data = MmioRead16 (Base + Offset);
  MmioWrite16 (Base + Offset, Data);
  return Data;
}

VOID
MtkMusbReadFifo (
  UINTN  Base,
  UINT8  EpNum,
  UINTN  Length,
  VOID   *Buffer
  )
{
  UINTN   FifoAddress;
  UINT32  *Aligned;
  UINT8   *Tail;
  UINTN   Whole;

  if ((Buffer == NULL) || (Length == 0)) {
    return;
  }

  FifoAddress = Base + MUSB_FIFO_OFFSET (EpNum);
  Aligned     = (UINT32 *)Buffer;

  while (((UINTN)Aligned & (sizeof (UINT32) - 1)) != 0) {
    *(UINT8 *)Aligned = MmioRead8 (FifoAddress);
    Aligned           = (UINT32 *)((UINT8 *)Aligned + 1);
    Length--;
    if (Length == 0) {
      return;
    }
  }

  Whole = Length / sizeof (UINT32);
  while (Whole-- > 0) {
    *Aligned++ = MmioRead32 (FifoAddress);
  }

  Tail   = (UINT8 *)Aligned;
  Length = Length % sizeof (UINT32);
  while (Length-- > 0) {
    *Tail++ = MmioRead8 (FifoAddress);
  }
}

VOID
MtkMusbWriteFifo (
  UINTN       Base,
  UINT8       EpNum,
  UINTN       Length,
  CONST VOID  *Buffer
  )
{
  UINTN         FifoAddress;
  CONST UINT32  *Aligned;
  CONST UINT8   *Tail;
  UINTN         Whole;

  if ((Buffer == NULL) || (Length == 0)) {
    return;
  }

  FifoAddress = Base + MUSB_FIFO_OFFSET (EpNum);
  Aligned     = (CONST UINT32 *)Buffer;

  while (((UINTN)Aligned & (sizeof (UINT32) - 1)) != 0) {
    MmioWrite8 (FifoAddress, *(CONST UINT8 *)Aligned);
    Aligned = (CONST UINT32 *)((CONST UINT8 *)Aligned + 1);
    Length--;
    if (Length == 0) {
      return;
    }
  }

  Whole = Length / sizeof (UINT32);
  while (Whole-- > 0) {
    MmioWrite32 (FifoAddress, *Aligned++);
  }

  Tail   = (CONST UINT8 *)Aligned;
  Length = Length % sizeof (UINT32);
  while (Length-- > 0) {
    MmioWrite8 (FifoAddress, *Tail++);
  }
}

VOID
MtkMusbSoftReset (
  UINTN  Base
  )
{
  /* Sequence from the U-Boot MT6768 glue platform_init. */
  MmioWrite8 (Base + MUSB_FADDR, 0);

  MmioWrite8 (Base + MUSB_POWER, MUSB_POWER_RESET);
  MicroSecondDelay (10);
  MmioWrite8 (Base + MUSB_POWER, 0);
  MicroSecondDelay (100);

  /* MediaTek MUSB_SWRST: controller and PHY signal gate reset. */
  MmioWrite8 (Base + MUSB_SWRST, MUSB_SWRST_SWRST | MUSB_SWRST_PHYSIG_GATE_EN);
  MicroSecondDelay (10);
  MmioWrite8 (Base + MUSB_SWRST, 0);
  MicroSecondDelay (100);
}

VOID
MtkMusbInitInterrupts (
  UINTN  Base
  )
{
  /* Vendor mt_usb_init / U-Boot mtk_musb_platform_init: unmask the L1
   * decoder for TX/RX/USBCOM/DMA.  QMU (bit 4) stays masked: the UEFI
   * bring-up uses PIO only. */
  MmioWrite32 (Base + USB_L1INTM,
               TX_INT_STATUS | RX_INT_STATUS | USBCOM_INT_STATUS | DMA_INT_STATUS);

  /* Enable data-toggle tracking for every endpoint. */
  MmioWrite16 (Base + MUSB_TXTOGEN, MTK_TOGGLE_EN);
  MmioWrite16 (Base + MUSB_RXTOGEN, MTK_TOGGLE_EN);
}

UINT32
MtkMusbGetL1Status (
  UINTN  Base
  )
{
  return MmioRead32 (Base + USB_L1INTS) & MmioRead32 (Base + USB_L1INTM);
}

VOID
MtkMusbAckCoreInterrupts (
  UINTN  Base,
  UINT8  *IntUsb,
  UINT16 *IntRx,
  UINT16 *IntTx
  )
{
  *IntUsb = MtkMusbClearByte (Base, MUSB_INTRUSB);
  *IntRx  = MtkMusbClearWord (Base, MUSB_INTRRX);
  *IntTx  = MtkMusbClearWord (Base, MUSB_INTRTX);
}

VOID
MtkMusbSnapshot (
  UINTN               Base,
  MTK_MUSB_SNAPSHOT   *Snapshot
  )
{
  if (Snapshot == NULL) {
    return;
  }

  Snapshot->Power        = MmioRead8 (Base + MUSB_POWER);
  Snapshot->Devctl       = MmioRead8 (Base + MUSB_DEVCTL);
  Snapshot->IntrUsb      = MmioRead8 (Base + MUSB_INTRUSB);
  Snapshot->IntrUsbEnable = MmioRead8 (Base + MUSB_INTRUSBE);
  Snapshot->IntrTx       = MmioRead16 (Base + MUSB_INTRTX);
  Snapshot->IntrTxe      = MmioRead16 (Base + MUSB_INTRTXE);
  Snapshot->IntrRx       = MmioRead16 (Base + MUSB_INTRRX);
  Snapshot->IntrRxe      = MmioRead16 (Base + MUSB_INTRRXE);
  Snapshot->Frame        = MmioRead16 (Base + MUSB_FRAME);
  Snapshot->Index        = MmioRead8 (Base + MUSB_INDEX);
  Snapshot->TestMode     = MmioRead8 (Base + MUSB_TESTMODE);
  Snapshot->L1Ints       = MmioRead32 (Base + USB_L1INTS);
  Snapshot->L1Intm       = MmioRead32 (Base + USB_L1INTM);
  Snapshot->SwRst        = MmioRead8 (Base + MUSB_SWRST);
  Snapshot->EpInfo       = MmioRead8 (Base + MUSB_EPINFO);
  Snapshot->RamInfo      = MmioRead8 (Base + MUSB_RAMINFO);
  Snapshot->HwVers       = MmioRead16 (Base + MUSB_HWVERS);

  Snapshot->Ep0Csr = MtkMusbReadIndexed16 (Base, 0, MUSB_CSR0);
  Snapshot->Ep1TxMaxp  = MtkMusbReadIndexed16 (Base, 1, MUSB_TXMAXP);
  Snapshot->Ep1Txcsr   = MtkMusbReadIndexed16 (Base, 1, MUSB_TXCSR);
  Snapshot->Ep1RxMaxp  = MtkMusbReadIndexed16 (Base, 1, MUSB_RXMAXP);
  Snapshot->Ep1Rxcsr   = MtkMusbReadIndexed16 (Base, 1, MUSB_RXCSR);
  Snapshot->Ep1RxCount = MtkMusbReadIndexed16 (Base, 1, MUSB_RXCOUNT);
}

VOID
MtkMusbPrintSnapshot (
  UINTN                     Base,
  CONST MTK_MUSB_SNAPSHOT   *Snapshot
  )
{
  if (Snapshot == NULL) {
    return;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: base=0x%lx HWVERS=%04x POWER=%02x DEVCTL=%02x FRAME=%04x\n",
    __FUNCTION__,
    (UINT64)Base,
    Snapshot->HwVers,
    Snapshot->Power,
    Snapshot->Devctl,
    Snapshot->Frame
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: INTRUSB=%02x/%02x INTRTX=%04x/%04x INTRRX=%04x/%04x L1=%08x/%08x\n",
    __FUNCTION__,
    Snapshot->IntrUsb,
    Snapshot->IntrUsbEnable,
    Snapshot->IntrTx,
    Snapshot->IntrTxe,
    Snapshot->IntrRx,
    Snapshot->IntrRxe,
    Snapshot->L1Ints,
    Snapshot->L1Intm
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: IDX=%02x TEST=%02x EPINFO=%02x RAMINFO=%02x SWRST=%02x EP0CSR=%04x\n",
    __FUNCTION__,
    Snapshot->Index,
    Snapshot->TestMode,
    Snapshot->EpInfo,
    Snapshot->RamInfo,
    Snapshot->SwRst,
    Snapshot->Ep0Csr
    ));
  DEBUG ((
    DEBUG_INFO,
    "%a: EP1 TXMAXP=%04x TXCSR=%04x RXMAXP=%04x RXCSR=%04x RXCOUNT=%04x\n",
    __FUNCTION__,
    Snapshot->Ep1TxMaxp,
    Snapshot->Ep1Txcsr,
    Snapshot->Ep1RxMaxp,
    Snapshot->Ep1Rxcsr,
    Snapshot->Ep1RxCount
    ));
}
