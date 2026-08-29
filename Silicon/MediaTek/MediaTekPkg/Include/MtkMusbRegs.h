/** @file
  Register definitions for the Mentor MUSB HDRC controller wrapped by
  MediaTek, as integrated in MT6768 (Barley / TB330XU).

  Sources (cross-checked against each other, see Docs/USB-HARDWARE-MANIFEST.md):
   - Vendor alps kernel drivers/misc/mediatek/usb20/mtk_musb_reg.h
     (MT6761/MT6765 generation, same USB IP; ZTE MT6761 GPL tree)
   - U-Boot MT6768 port drivers/usb/musb-new/mt6768.c (boots fastboot
     over USB on this exact SoC)
   - Upstream Linux drivers/usb/musb/musb_regs.h (core MUSB bits)

  Copyright (C) 2026, TB330XU bring-up project.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MTK_MUSB_REGS_H_
#define MTK_MUSB_REGS_H_

#define MTK_MUSB_BASE              0x11200000
#define MTK_MUSB_BASE_LENGTH       SIZE_4KB

#define MTK_MUSB_NUM_EPS           16   /* MUSB_C_NUM_EPS, vendor musb_core.h */
#define MTK_MUSB_EP0_MAX_PACKET    64   /* ep0_cfg, vendor musb_core.c */

/*
 * Common core registers.  Widths matter: all EP-scoped registers are
 * reached through MUSB_INDEX; common ones are addressed directly.
 */
#define MUSB_FADDR                 0x00  /* 8-bit */
#define MUSB_POWER                 0x01  /* 8-bit */

#define MUSB_INTRTX                0x02  /* 16-bit */
#define MUSB_INTRRX                0x04  /* 16-bit */
#define MUSB_INTRTXE               0x06  /* 16-bit */
#define MUSB_INTRRXE               0x08  /* 16-bit */
#define MUSB_INTRUSB               0x0A  /* 8-bit */
#define MUSB_INTRUSBE              0x0B  /* 8-bit */
#define MUSB_FRAME                 0x0C  /* 16-bit */
#define MUSB_INDEX                 0x0E  /* 8-bit */
#define MUSB_TESTMODE              0x0F  /* 8-bit */

#define MUSB_FIFO_OFFSET(epnum)    (0x20 + ((epnum) * 4))

#define MUSB_DEVCTL                0x60  /* 8-bit */

/* Indexed (INDEX-selected) endpoint register offsets. */
#define MUSB_TXMAXP                0x00
#define MUSB_TXCSR                 0x02
#define MUSB_CSR0                  MUSB_TXCSR
#define MUSB_RXMAXP                0x04
#define MUSB_RXCSR                 0x06
#define MUSB_RXCOUNT               0x08
#define MUSB_COUNT0                MUSB_RXCOUNT
#define MUSB_TXTYPE                0x0A
#define MUSB_TYPE0                 MUSB_TXTYPE
#define MUSB_TXINTERVAL            0x0B
#define MUSB_NAKLIMIT0             MUSB_TXINTERVAL
#define MUSB_RXTYPE                0x0C
#define MUSB_RXINTERVAL            0x0D
#define MUSB_FIFOSIZE              0x0F
#define MUSB_CONFIGDATA            MUSB_FIFOSIZE

#define MUSB_INDEXED_OFFSET(epnum, offset)  (0x10 + (offset))

/* Bus control / multipoint host target registers. */
#define MUSB_TXFUNCADDR            0x0480
#define MUSB_TXHUBADDR             0x0482
#define MUSB_RXFUNCADDR            0x0484
#define MUSB_RXHUBADDR             0x0486

#define MUSB_BUSCTL_OFFSET(epnum, offset)  (0x80 + (8 * (epnum)) + (offset))

/* MediaTek data-toggle registers (16-bit). */
#define MUSB_RXTOG                 0x0080
#define MUSB_RXTOGEN               0x0082
#define MUSB_TXTOG                 0x0084
#define MUSB_TXTOGEN               0x0086
#define MTK_TOGGLE_EN              0xFFFF

#define MUSB_HWVERS                0x6C  /* 8-bit */
#define MUSB_EPINFO                0x78  /* 8-bit: endpoint count / RAM info */
#define MUSB_RAMINFO               0x79  /* 8-bit */
#define MUSB_LINKINFO              0x7A
#define MUSB_VPLEN                 0x7B
#define MUSB_HS_EOF1               0x7C
#define MUSB_FS_EOF1               0x7D
#define MUSB_LS_EOF1               0x7E

/* MediaTek software reset (mtk_musb_reg.h:305). */
#define MUSB_SWRST                 0x74
#define MUSB_SWRST_PHY_RST         (1 << 7)
#define MUSB_SWRST_PHYSIG_GATE_HS  (1 << 6)
#define MUSB_SWRST_PHYSIG_GATE_EN  (1 << 5)
#define MUSB_SWRST_REDUCE_DLY      (1 << 4)
#define MUSB_SWRST_UNDO_SRPFIX     (1 << 3)
#define MUSB_SWRST_FRC_VBUSVALID   (1 << 2)
#define MUSB_SWRST_SWRST           (1 << 1)
#define MUSB_SWRST_DISUSBRESET     (1 << 0)

/* MediaTek level-1 interrupt decoder (mtk_musb_reg.h:312-314). */
#define USB_L1INTS                 0x00A0
#define USB_L1INTM                 0x00A4
#define USB_L1INTP                 0x00A8

#define TX_INT_STATUS              (1 << 0)
#define RX_INT_STATUS              (1 << 1)
#define USBCOM_INT_STATUS          (1 << 2)
#define DMA_INT_STATUS             (1 << 3)
#define QINT_STATUS                (1 << 4)  /* QMU, vendor usb20.c */

#define MUSB_USB_MDL1INTM          0x0744
#define MUSB_USBGCSR               0x0B00
#define MUSB_QIMCR                 0x0C08
#define MUSB_QIMSR                 0x0C0C

/* Classic MUSB HS DMA (vendor; PIO first, DMA deferred). */
#define MUSB_HSDMA_BASE            0x0200
#define MUSB_HSDMA_CHANNEL(n)      (MUSB_HSDMA_BASE + (0x10 * (n)))
#define MUSB_DMA_REALCOUNT(chan)   (0x0280 + (0x10 * (chan)))
#define DMA_INTR_UNMASK_CLR_OFFSET 16
#define DMA_INTR_UNMASK_SET_OFFSET 24
#define MUSB_HSDMA_INTR            (MUSB_HSDMA_BASE + 0x08)

/* MUSB_POWER bits */
#define MUSB_POWER_ISOUPDATE       0x80
#define MUSB_POWER_SOFTCONN        0x40
#define MUSB_POWER_HSENAB          0x20
#define MUSB_POWER_HSMODE          0x10
#define MUSB_POWER_RESET           0x08
#define MUSB_POWER_RESUME          0x04
#define MUSB_POWER_SUSPENDM        0x02
#define MUSB_POWER_ENSUSPEND       0x01

/* MUSB_INTRUSB bits */
#define MUSB_INTR_SUSPEND          0x01
#define MUSB_INTR_RESUME           0x02
#define MUSB_INTR_RESET            0x04
#define MUSB_INTR_BABBLE           0x04
#define MUSB_INTR_SOF              0x08
#define MUSB_INTR_CONNECT          0x10
#define MUSB_INTR_DISCONNECT       0x20
#define MUSB_INTR_SESSREQ          0x40
#define MUSB_INTR_VBUSERROR        0x80

/* MUSB_DEVCTL bits */
#define MUSB_DEVCTL_BDEVICE        0x80
#define MUSB_DEVCTL_FSDEV          0x40
#define MUSB_DEVCTL_LSDEV          0x20
#define MUSB_DEVCTL_VBUS           0x18
#define MUSB_DEVCTL_VBUS_SHIFT     3
#define MUSB_DEVCTL_HM             0x04
#define MUSB_DEVCTL_HR             0x02
#define MUSB_DEVCTL_SESSION        0x01

/* MUSB_TESTMODE bits */
#define MUSB_TEST_FORCE_HOST       0x80
#define MUSB_TEST_FIFO_ACCESS      0x40
#define MUSB_TEST_FORCE_FS         0x20
#define MUSB_TEST_FORCE_HS         0x10
#define MUSB_TEST_PACKET           0x08
#define MUSB_TEST_K                0x04
#define MUSB_TEST_J                0x02
#define MUSB_TEST_SE0_NAK          0x01

/* EP0 CSR0 bits (peripheral) */
#define MUSB_CSR0_FLUSHFIFO        0x0100
#define MUSB_CSR0_TXPKTRDY         0x0002
#define MUSB_CSR0_RXPKTRDY         0x0001
#define MUSB_CSR0_P_SVDSETUPEND    0x0080
#define MUSB_CSR0_P_SVDRXPKTRDY    0x0040
#define MUSB_CSR0_P_SENDSTALL      0x0020
#define MUSB_CSR0_P_SETUPEND       0x0010
#define MUSB_CSR0_P_DATAEND        0x0008
#define MUSB_CSR0_P_SENTSTALL      0x0004
#define MUSB_CSR0_P_WZC_BITS       (MUSB_CSR0_P_SENTSTALL)

/* EP0 CSR0 bits (host) */
#define MUSB_CSR0_H_DIS_PING       0x0800
#define MUSB_CSR0_H_WR_DATATOGGLE  0x0400
#define MUSB_CSR0_H_DATATOGGLE     0x0200
#define MUSB_CSR0_H_NAKTIMEOUT     0x0080
#define MUSB_CSR0_H_STATUSPKT      0x0040
#define MUSB_CSR0_H_REQPKT         0x0020
#define MUSB_CSR0_H_ERROR          0x0010
#define MUSB_CSR0_H_SETUPPKT       0x0008
#define MUSB_CSR0_H_RXSTALL        0x0004
#define MUSB_CSR0_H_WZC_BITS       \
  (MUSB_CSR0_H_NAKTIMEOUT | MUSB_CSR0_H_RXSTALL | MUSB_CSR0_RXPKTRDY)

/* TXTYPE/RXTYPE fields */
#define MUSB_TYPE_SPEED            0xC0
#define MUSB_TYPE_SPEED_SHIFT      6
#define MUSB_TYPE_PROTO            0x30
#define MUSB_TYPE_PROTO_SHIFT      4
#define MUSB_TYPE_REMOTE_END       0x0F

/* TXCSR bits (peripheral) */
#define MUSB_TXCSR_AUTOSET         0x8000
#define MUSB_TXCSR_ISO             0x4000
#define MUSB_TXCSR_DMAENAB         0x1000
#define MUSB_TXCSR_FRCDATATOG      0x0800
#define MUSB_TXCSR_DMAMODE         0x0400
#define MUSB_TXCSR_MODE            0x2000  /* host-mode CSR bit (mtk_musb_reg.h) */
#define MUSB_TXCSR_CLRDATATOG      0x0040
#define MUSB_TXCSR_FLUSHFIFO       0x0008
#define MUSB_TXCSR_FIFONOTEMPTY    0x0002
#define MUSB_TXCSR_TXPKTRDY        0x0001

/* TXCSR peripheral-only bits */
#define MUSB_TXCSR_P_ISO           0x4000
#define MUSB_TXCSR_P_INCOMPTX      0x0100
#define MUSB_TXCSR_P_SENTSTALL     0x0020
#define MUSB_TXCSR_P_UNDERRUN      0x0010
#define MUSB_TXCSR_P_DATATOGGLE    0x0008  /* R */

/* TXCSR host-only bits */
#define MUSB_TXCSR_H_NAKTIMEOUT    0x0080
#define MUSB_TXCSR_H_ERROR         0x0004
#define MUSB_TXCSR_H_DATATOGGLE    0x0008
#define MUSB_TXCSR_H_WR_DATATOGGLE 0x0002

#define MUSB_TXCSR_H_WZC_BITS      \
  (MUSB_TXCSR_H_NAKTIMEOUT | MUSB_TXCSR_H_ERROR | MUSB_TXCSR_FIFONOTEMPTY)
#define MUSB_TXCSR_P_WZC_BITS      \
  (MUSB_TXCSR_P_INCOMPTX | MUSB_TXCSR_P_SENTSTALL | MUSB_TXCSR_P_UNDERRUN)

/* RXCSR bits (peripheral) */
#define MUSB_RXCSR_AUTOCLEAR       0x8000
#define MUSB_RXCSR_DMAENAB         0x2000
#define MUSB_RXCSR_DISNYET         0x1000
#define MUSB_RXCSR_PID_ERR         0x0100
#define MUSB_RXCSR_DATATOGGLE      0x0080
#define MUSB_RXCSR_CLRDATATOG      0x0080
#define MUSB_RXCSR_FLUSHFIFO       0x0010
#define MUSB_RXCSR_DATAERROR       0x0008
#define MUSB_RXCSR_FIFOFULL        0x0002
#define MUSB_RXCSR_RXPKTRDY        0x0001

#define MUSB_RXCSR_P_ISO           0x4000
#define MUSB_RXCSR_P_SENTSTALL     0x0040
#define MUSB_RXCSR_P_OVERRUN       0x0004

/* RXCSR host-only bits */
#define MUSB_RXCSR_H_AUTOREQ       0x4000
#define MUSB_RXCSR_H_REQPKT        0x0020
#define MUSB_RXCSR_H_NAKTIMEOUT    0x0004
#define MUSB_RXCSR_H_ERROR         0x0004

#define MUSB_RXCSR_H_WZC_BITS      \
  (MUSB_RXCSR_H_NAKTIMEOUT | MUSB_RXCSR_H_ERROR | MUSB_RXCSR_RXPKTRDY)
#define MUSB_RXCSR_P_WZC_BITS      \
  (MUSB_RXCSR_P_SENTSTALL | MUSB_RXCSR_P_OVERRUN)

/* FIFO size register encoding (TXFIFOSZ/RXFIFOSZ) */
#define MUSB_FIFOSZ_8              0x00
#define MUSB_FIFOSZ_16             0x01
#define MUSB_FIFOSZ_32             0x02
#define MUSB_FIFOSZ_64             0x03
#define MUSB_FIFOSZ_128            0x04
#define MUSB_FIFOSZ_256            0x05
#define MUSB_FIFOSZ_512            0x06
#define MUSB_FIFOSZ_1024           0x07
#define MUSB_FIFOSZ_DPBM           0x10  /* double-packet buffer mode */

#endif /* MTK_MUSB_REGS_H_ */
