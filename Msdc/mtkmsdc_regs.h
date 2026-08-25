#pragma once

/*
 * MT6768 MSDC register definitions used by the diagnostic SDPORT miniport.
 * Values are shared with Mu-Silicium MsdcDxe and Linux mtk-sd.c.
 */

#define MTK_BIT(_n) (1UL << (_n))

#define MSDC_CFG                0x000
#define MSDC_IOCON              0x004
#define MSDC_PS                 0x008
#define MSDC_INT                0x00c
#define MSDC_INTEN              0x010
#define MSDC_FIFOCS             0x014
#define MSDC_TXDATA             0x018
#define MSDC_RXDATA             0x01c
#define SDC_CFG                 0x030
#define SDC_CMD                 0x034
#define SDC_ARG                 0x038
#define SDC_STS                 0x03c
#define SDC_RESP0               0x040
#define SDC_RESP1               0x044
#define SDC_RESP2               0x048
#define SDC_RESP3               0x04c
#define SDC_BLK_NUM             0x050
#define MSDC_PATCH_BIT0         0x0b0
#define MSDC_PATCH_BIT1         0x0b4
#define MSDC_PATCH_BIT2         0x0b8
#define EMMC50_CFG0             0x208
#define SDC_FIFO_CFG            0x228

#define MSDC_CFG_MODE           MTK_BIT(0)
#define MSDC_CFG_CKPD           MTK_BIT(1)
#define MSDC_CFG_RST            MTK_BIT(2)
#define MSDC_CFG_PIO            MTK_BIT(3)
#define MSDC_CFG_CKSTB          MTK_BIT(7)
#define MSDC_CFG_CKDIV_MASK     (0xfffUL << 8)
#define MSDC_CFG_CKMOD_MASK     (3UL << 20)
#define MSDC_CFG_HS400_CK_MODE  MTK_BIT(22)

#define MSDC_PS_CDEN            MTK_BIT(0)
#define MSDC_PS_CDSTS           MTK_BIT(1)
#define MSDC_PS_WP              MTK_BIT(31)

#define MSDC_INT_MMCIRQ         MTK_BIT(0)
#define MSDC_INT_CDSC           MTK_BIT(1)
#define MSDC_INT_ACMDRDY        MTK_BIT(3)
#define MSDC_INT_ACMDTMO        MTK_BIT(4)
#define MSDC_INT_ACMDCRCERR      MTK_BIT(5)
#define MSDC_INT_CMDRDY          MTK_BIT(8)
#define MSDC_INT_CMDTMO          MTK_BIT(9)
#define MSDC_INT_CMDCRCERR       MTK_BIT(10)
#define MSDC_INT_XFER_COMPL      MTK_BIT(12)
#define MSDC_INT_DATTMO          MTK_BIT(14)
#define MSDC_INT_DATCRCERR       MTK_BIT(15)
#define MSDC_INT_BDCSERR         MTK_BIT(17)
#define MSDC_INT_GPDCSERR        MTK_BIT(18)

#define MSDC_INT_CMD_ERROR      (MSDC_INT_CMDTMO | MSDC_INT_CMDCRCERR)
#define MSDC_INT_ACMD_ERROR     (MSDC_INT_ACMDTMO | MSDC_INT_ACMDCRCERR)
#define MSDC_INT_DATA_ERROR     (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR | \
                                 MSDC_INT_BDCSERR | MSDC_INT_GPDCSERR)
#define MSDC_INT_CMD_STATUS     (MSDC_INT_CMDRDY | MSDC_INT_CMD_ERROR)
#define MSDC_INT_DATA_STATUS    (MSDC_INT_XFER_COMPL | MSDC_INT_DATA_ERROR | \
                                 MSDC_INT_ACMD_ERROR)

#define MSDC_FIFOCS_RXCNT_MASK  0x000000ffUL
#define MSDC_FIFOCS_TXCNT_MASK  0x00ff0000UL
#define MSDC_FIFOCS_TXCNT_SHIFT 16
#define MSDC_FIFOCS_CLR         MTK_BIT(31)
#define MSDC_FIFO_SIZE          128UL

#define SDC_CFG_BUS_WIDTH_SHIFT 16
#define SDC_CFG_BUS_WIDTH_MASK  (3UL << SDC_CFG_BUS_WIDTH_SHIFT)
#define SDC_CFG_SDIO            MTK_BIT(19)
#define SDC_CFG_SDIOIDE         MTK_BIT(20)
#define SDC_CFG_DTOC_MASK       (0xffUL << 24)

#define MSDC_BUS_WIDTH_1        0UL
#define MSDC_BUS_WIDTH_4        1UL
#define MSDC_BUS_WIDTH_8        2UL

#define SDC_CMD_RSP_TYPE_SHIFT  7
#define SDC_CMD_SINGLE_BLK      MTK_BIT(11)
#define SDC_CMD_MULTIPLE_BLK    MTK_BIT(12)
#define SDC_CMD_RW              MTK_BIT(13)
#define SDC_CMD_STOP_CMD        MTK_BIT(14)
#define SDC_CMD_BLK_SIZE_SHIFT  16
#define SDC_CMD_AUTO12          MTK_BIT(28)

#define SDC_STS_SDCBUSY         MTK_BIT(0)
#define SDC_STS_CMDBUSY         MTK_BIT(1)

#define EMMC50_CFG0_CRCSTSSEL   MTK_BIT(4)
#define MSDC_PATCH1_BUSY_CHECK  MTK_BIT(7)
#define SDC_FIFO_CFG_WRVALIDSEL MTK_BIT(24)
#define SDC_FIFO_CFG_RDVALIDSEL MTK_BIT(25)

#define MTK_MSDC_REQUIRED_MMIO_LENGTH 0x22cUL
#define MTK_MSDC_SOURCE_CLOCK_HZ      320000000UL
#define MTK_MSDC_MAX_CLOCK_HZ          25000000UL

