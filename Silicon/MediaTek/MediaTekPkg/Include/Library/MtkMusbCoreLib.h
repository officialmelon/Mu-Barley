/** @file
  Public interface of the MediaTek MUSB core access library.

  Shared by the MUSB host and device DXE drivers.  The library is reentrant;
  every call carries the controller base address.  Register values and
  sequences are sourced from the vendor alps MUSB and the U-Boot MT6768
  glue (see Docs/USB-HARDWARE-MANIFEST.md).

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MTK_MUSB_CORE_LIB_H_
#define MTK_MUSB_CORE_LIB_H_

#include <Uefi.h>
#include <MtkMusbRegs.h>

/**
  Raw register access (core and wrapper space).
**/
UINT8   MtkMusbRead8  (UINTN Base, UINTN Offset);
UINT16  MtkMusbRead16 (UINTN Base, UINTN Offset);
UINT32  MtkMusbRead32 (UINTN Base, UINTN Offset);
VOID    MtkMusbWrite8  (UINTN Base, UINTN Offset, UINT8 Value);
VOID    MtkMusbWrite16 (UINTN Base, UINTN Offset, UINT16 Value);
VOID    MtkMusbWrite32 (UINTN Base, UINTN Offset, UINT32 Value);

/**
  Select the indexed endpoint (MUSB_INDEX) and wait is not required by the
  hardware; subsequent indexed accesses hit the selected endpoint.
**/
VOID    MtkMusbSelectEndpoint (UINTN Base, UINT8 EpNum);

/**
  Indexed register access: selects the endpoint then reads/writes the
  given endpoint-relative offset.
**/
UINT8   MtkMusbReadIndexed8  (UINTN Base, UINT8 EpNum, UINTN Offset);
UINT16  MtkMusbReadIndexed16 (UINTN Base, UINT8 EpNum, UINTN Offset);
VOID    MtkMusbWriteIndexed8  (UINTN Base, UINT8 EpNum, UINTN Offset, UINT8 Value);
VOID    MtkMusbWriteIndexed16 (UINTN Base, UINT8 EpNum, UINTN Offset, UINT16 Value);

/**
  Clear-then-write W1C helpers used by the MediaTek wrapper for
  INTRTX/INTRRX/INTRUSB acking (mtk_musb_clearb/clearw in U-Boot glue).
**/
UINT8   MtkMusbClearByte (UINTN Base, UINTN Offset);
UINT16  MtkMusbClearWord (UINTN Base, UINTN Offset);

/**
  PIO FIFO access.  Performs 32-bit moves where the buffer alignment
  allows, then byte tail.
**/
VOID    MtkMusbReadFifo  (UINTN Base, UINT8 EpNum, UINTN Length, VOID *Buffer);
VOID    MtkMusbWriteFifo (UINTN Base, UINT8 EpNum, UINTN Length, CONST VOID *Buffer);

/**
  Controller soft reset: MUSB_POWER RESET pulse (10 us) followed by the
  MediaTek MUSB_SWRST register with the vendor-recommended bits.  Callers
  must re-run interrupt and endpoint configuration afterwards.
**/
VOID    MtkMusbSoftReset (UINTN Base);

/**
  Common interrupt bring-up used by both host and device paths, matching
  the vendor mt_usb_init / mtk_musb_platform_init:
    - unmask L1 TX|RX|USBCOM|DMA (QINT intentionally masked while PIO-only)
    - enable TX/RX toggle tracking for all endpoints
  Does NOT touch DEVCTL/POWER (role-specific).
**/
VOID    MtkMusbInitInterrupts (UINTN Base);

/**
  Read the level-1 interrupt status (already ANDed with the mask).
**/
UINT32  MtkMusbGetL1Status (UINTN Base);

VOID    MtkMusbAckCoreInterrupts (UINTN Base, UINT8 *IntUsb, UINT16 *IntRx, UINT16 *IntTx);

/**
  Diagnostic snapshot: raw register dump of the controller core and the
  wrapper, for on-screen bring-up logging.  Fills a caller-provided buffer
  and optionally prints through DEBUG().
**/
typedef struct {
  UINT8   Power;
  UINT8   Devctl;
  UINT8   IntrUsb;
  UINT8   IntrUsbEnable;
  UINT16  IntrTx;
  UINT16  IntrTxe;
  UINT16  IntrRx;
  UINT16  IntrRxe;
  UINT16  Frame;
  UINT8   Index;
  UINT8   TestMode;
  UINT32  L1Ints;
  UINT32  L1Intm;
  UINT8   SwRst;
  UINT8   EpInfo;
  UINT8   RamInfo;
  UINT16  HwVers;
  UINT16  Ep0Csr;
  UINT16  Ep1TxMaxp;
  UINT16  Ep1Txcsr;
  UINT16  Ep1RxMaxp;
  UINT16  Ep1Rxcsr;
  UINT16  Ep1RxCount;
} MTK_MUSB_SNAPSHOT;

VOID    MtkMusbSnapshot (UINTN Base, MTK_MUSB_SNAPSHOT *Snapshot);
VOID    MtkMusbPrintSnapshot (UINTN Base, CONST MTK_MUSB_SNAPSHOT *Snapshot);

#endif /* MTK_MUSB_CORE_LIB_H_ */
