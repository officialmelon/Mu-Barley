/*
 * MediaTek MT6765/MT6768 SPI0 PIO transport used by Barley's HX83102J.
 *
 * This is a small, polling implementation of the same FIFO state machine as
 * Linux drivers/spi/spi-mt65xx.c.  It deliberately avoids DMA and interrupts:
 * touch traffic is tiny, while firmware writes are segmented through the
 * controller's PAUSE/RESUME path with chip-select kept asserted.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "barley_touch.h"

#define MTK_SPI_CFG0                 0x0000U
#define MTK_SPI_CFG1                 0x0004U
#define MTK_SPI_TX_DATA              0x0010U
#define MTK_SPI_RX_DATA              0x0014U
#define MTK_SPI_CMD                  0x0018U
#define MTK_SPI_STATUS0              0x001CU
#define MTK_SPI_PAD_SEL              0x0024U
#define MTK_SPI_CFG2                 0x0028U

#define MTK_SPI_CMD_ACT              (1U << 0)
#define MTK_SPI_CMD_RESUME           (1U << 1)
#define MTK_SPI_CMD_RST              (1U << 2)
#define MTK_SPI_CMD_PAUSE_EN         (1U << 4)
#define MTK_SPI_CMD_DEASSERT         (1U << 5)
#define MTK_SPI_CMD_SAMPLE_SEL       (1U << 6)
#define MTK_SPI_CMD_CS_POL           (1U << 7)
#define MTK_SPI_CMD_CPHA             (1U << 8)
#define MTK_SPI_CMD_CPOL             (1U << 9)
#define MTK_SPI_CMD_RX_DMA           (1U << 10)
#define MTK_SPI_CMD_TX_DMA           (1U << 11)
#define MTK_SPI_CMD_TXMSBF           (1U << 12)
#define MTK_SPI_CMD_RXMSBF           (1U << 13)
#define MTK_SPI_CMD_RX_ENDIAN        (1U << 14)
#define MTK_SPI_CMD_TX_ENDIAN        (1U << 15)
#define MTK_SPI_CMD_FINISH_IE        (1U << 16)
#define MTK_SPI_CMD_PAUSE_IE         (1U << 17)

#define MTK_SPI_STATUS_FINISH        0x1U
#define MTK_SPI_STATUS_PAUSE         0x2U
#define MTK_SPI_FIFO_SIZE            32U
#define MTK_SPI_PACKET_LENGTH_MASK   0x03FF0000U
#define MTK_SPI_PACKET_LOOP_MASK     0x0000FF00U

#define MTK_TOP_SPI_MUX              0x0060U
#define MTK_TOP_SPI_CLEAR            0x0068U
#define MTK_TOP_CLK_CFG_UPDATE       0x0004U
#define MTK_TOP_SPI_GATE_BIT         (1U << 31)
#define MTK_TOP_SPI_UPDATE_BIT       (1U << 11)
#define MTK_TOP_SPI_PARENT_SHIFT     24U
#define MTK_TOP_SPI_PARENT_MASK      0x3U

#define MTK_INFRA_CG1_CLEAR          0x008CU
#define MTK_INFRA_CG1_STATUS         0x0094U
#define MTK_INFRA_SPI0_GATE_BIT      (1U << 1)

#define MTK_GPIO_RST_MODE_OFFSET     0x03B0U
#define MTK_GPIO_RST_MODE_MASK       (0xFU << 16)
#define MTK_GPIO_RST_DIR_OFFSET      0x0020U
#define MTK_GPIO_RST_DOUT_OFFSET     0x0120U
#define MTK_GPIO_RST_BIT             (1U << 28)

#define MTK_SPI_POLL_LIMIT           100000U

static __forceinline ULONG
MtkRead32(
    _In_ PUCHAR Base,
    _In_ ULONG Offset
    )
{
    return READ_REGISTER_ULONG((PULONG)(Base + Offset));
}

static __forceinline VOID
MtkWrite32(
    _In_ PUCHAR Base,
    _In_ ULONG Offset,
    _In_ ULONG Value
    )
{
    WRITE_REGISTER_ULONG((PULONG)(Base + Offset), Value);
}

static VOID
MtkSpiControllerReset(
    _Inout_ PBARLEY_MTK_SPI Spi
    )
{
    ULONG command;

    command = MtkRead32(Spi->Registers, MTK_SPI_CMD);
    MtkWrite32(Spi->Registers, MTK_SPI_CMD, command | MTK_SPI_CMD_RST);
    MtkWrite32(Spi->Registers, MTK_SPI_CMD, command & ~MTK_SPI_CMD_RST);
}

static ULONG
MtkSpiSourceClockFromMux(
    _In_ PBARLEY_MTK_SPI Spi
    )
{
    ULONG parent;

    parent = (MtkRead32(Spi->TopCkgen, MTK_TOP_SPI_MUX) >>
              MTK_TOP_SPI_PARENT_SHIFT) & MTK_TOP_SPI_PARENT_MASK;

    /* MT6768 topckgen parent rates from the common clock description. */
    switch (parent) {
    case 0:
        return 26000000U;
    case 1:
        return 109200000U;
    case 2:
        return 78000000U;
    case 3:
        return 91000000U;
    default:
        return 26000000U;
    }
}

static VOID
MtkConfigureResetOutputOnly(
    _Inout_ PBARLEY_MTK_SPI Spi
    )
{
    ULONG value;

    /* GPIO92 function 0, output, deasserted high. */
    value = MtkRead32(Spi->Gpio, MTK_GPIO_RST_MODE_OFFSET);
    value &= ~MTK_GPIO_RST_MODE_MASK;
    MtkWrite32(Spi->Gpio, MTK_GPIO_RST_MODE_OFFSET, value);

    value = MtkRead32(Spi->Gpio, MTK_GPIO_RST_DOUT_OFFSET);
    value |= MTK_GPIO_RST_BIT;
    MtkWrite32(Spi->Gpio, MTK_GPIO_RST_DOUT_OFFSET, value);

    value = MtkRead32(Spi->Gpio, MTK_GPIO_RST_DIR_OFFSET);
    value |= MTK_GPIO_RST_BIT;
    MtkWrite32(Spi->Gpio, MTK_GPIO_RST_DIR_OFFSET, value);
    KeMemoryBarrier();
}

NTSTATUS
BarleyMtkSpiInitialize(
    _Inout_ PBARLEY_MTK_SPI Spi
    )
{
    ULONG command;
    ULONG divider;
    ULONG halfPeriod;
    ULONG cfg2;

    if (Spi == NULL || Spi->Registers == NULL || Spi->TopCkgen == NULL ||
        Spi->InfraCfg == NULL || Spi->Gpio == NULL) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    /* Ungate the already-selected TOP_SPI parent and the SPI0 infra gate. */
    MtkWrite32(Spi->TopCkgen, MTK_TOP_SPI_CLEAR, MTK_TOP_SPI_GATE_BIT);
    MtkWrite32(Spi->TopCkgen, MTK_TOP_CLK_CFG_UPDATE,
               MTK_TOP_SPI_UPDATE_BIT);
    MtkWrite32(Spi->InfraCfg, MTK_INFRA_CG1_CLEAR,
               MTK_INFRA_SPI0_GATE_BIT);
    KeMemoryBarrier();
    KeStallExecutionProcessor(10);

    if ((MtkRead32(Spi->InfraCfg, MTK_INFRA_CG1_STATUS) &
         MTK_INFRA_SPI0_GATE_BIT) != 0) {
        return STATUS_DEVICE_POWER_FAILURE;
    }

    /* Preserve Lenovo LK's live SPI pinmux; only configure FDT-confirmed reset GPIO92. */
    MtkConfigureResetOutputOnly(Spi);
    Spi->SourceClockHz = MtkSpiSourceClockFromMux(Spi);

    MtkSpiControllerReset(Spi);

    command = MtkRead32(Spi->Registers, MTK_SPI_CMD);
    command &= ~(MTK_SPI_CMD_ACT | MTK_SPI_CMD_RESUME |
                 MTK_SPI_CMD_PAUSE_EN | MTK_SPI_CMD_DEASSERT |
                 MTK_SPI_CMD_SAMPLE_SEL | MTK_SPI_CMD_CS_POL |
                 MTK_SPI_CMD_CPOL | MTK_SPI_CMD_RX_DMA |
                 MTK_SPI_CMD_TX_DMA | MTK_SPI_CMD_RX_ENDIAN |
                 MTK_SPI_CMD_TX_ENDIAN | MTK_SPI_CMD_FINISH_IE |
                 MTK_SPI_CMD_PAUSE_IE);
    command |= MTK_SPI_CMD_CPHA | MTK_SPI_CMD_TXMSBF | MTK_SPI_CMD_RXMSBF;
    MtkWrite32(Spi->Registers, MTK_SPI_CMD, command);
    MtkWrite32(Spi->Registers, MTK_SPI_PAD_SEL, 0U);

    /* Never exceed the FDT's 10 MHz limit. */
    divider = (Spi->SourceClockHz + 9999999U) / 10000000U;
    if (divider < 2U) {
        divider = 2U;
    }
    halfPeriod = (divider + 1U) / 2U;
    if (halfPeriod == 0U) {
        halfPeriod = 1U;
    }
    cfg2 = ((halfPeriod - 1U) & 0xFFFFU) |
           (((halfPeriod - 1U) & 0xFFFFU) << 16);
    MtkWrite32(Spi->Registers, MTK_SPI_CFG2, cfg2);

    return STATUS_SUCCESS;
}

VOID
BarleyMtkSpiResetTouch(
    _Inout_ PBARLEY_MTK_SPI Spi
    )
{
    ULONG value;

    if (Spi == NULL || Spi->Gpio == NULL) {
        return;
    }

    value = MtkRead32(Spi->Gpio, MTK_GPIO_RST_DOUT_OFFSET);
    value &= ~MTK_GPIO_RST_BIT;
    MtkWrite32(Spi->Gpio, MTK_GPIO_RST_DOUT_OFFSET, value);
    KeMemoryBarrier();
    KeStallExecutionProcessor(20000);

    value |= MTK_GPIO_RST_BIT;
    MtkWrite32(Spi->Gpio, MTK_GPIO_RST_DOUT_OFFSET, value);
    KeMemoryBarrier();
    KeStallExecutionProcessor(20000);
}

NTSTATUS
BarleyMtkSpiTransfer(
    _Inout_ PBARLEY_MTK_SPI Spi,
    _In_reads_bytes_(Length) const UCHAR *Transmit,
    _Out_writes_bytes_opt_(Length) UCHAR *Receive,
    _In_ ULONG Length
    )
{
    ULONG command;
    ULONG cfg1;
    ULONG offset;
    ULONG chunk;
    ULONG wordOffset;
    ULONG bytes;
    ULONG word;
    ULONG poll;
    ULONG status;
    BOOLEAN paused;

    if (Spi == NULL || Spi->Registers == NULL || Transmit == NULL ||
        Length == 0U) {
        return STATUS_INVALID_PARAMETER;
    }

    MtkSpiControllerReset(Spi);
    command = MtkRead32(Spi->Registers, MTK_SPI_CMD);
    command &= ~(MTK_SPI_CMD_ACT | MTK_SPI_CMD_RESUME |
                 MTK_SPI_CMD_RX_DMA | MTK_SPI_CMD_TX_DMA);
    if (Length > MTK_SPI_FIFO_SIZE) {
        command |= MTK_SPI_CMD_PAUSE_EN;
    } else {
        command &= ~MTK_SPI_CMD_PAUSE_EN;
    }
    MtkWrite32(Spi->Registers, MTK_SPI_CMD, command);

    offset = 0U;
    paused = FALSE;
    while (offset < Length) {
        chunk = Length - offset;
        if (chunk > MTK_SPI_FIFO_SIZE) {
            chunk = MTK_SPI_FIFO_SIZE;
        }

        cfg1 = MtkRead32(Spi->Registers, MTK_SPI_CFG1);
        cfg1 &= ~(MTK_SPI_PACKET_LENGTH_MASK | MTK_SPI_PACKET_LOOP_MASK);
        cfg1 |= (chunk - 1U) << 16;
        MtkWrite32(Spi->Registers, MTK_SPI_CFG1, cfg1);

        for (wordOffset = 0U; wordOffset < chunk; wordOffset += sizeof(ULONG)) {
            word = 0U;
            bytes = chunk - wordOffset;
            if (bytes > sizeof(ULONG)) {
                bytes = sizeof(ULONG);
            }
            RtlCopyMemory(&word, Transmit + offset + wordOffset, bytes);
            MtkWrite32(Spi->Registers, MTK_SPI_TX_DATA, word);
        }

        command = MtkRead32(Spi->Registers, MTK_SPI_CMD);
        if (paused) {
            command |= MTK_SPI_CMD_RESUME;
        } else {
            command |= MTK_SPI_CMD_ACT;
        }
        MtkWrite32(Spi->Registers, MTK_SPI_CMD, command);

        status = 0U;
        for (poll = 0U; poll < MTK_SPI_POLL_LIMIT; ++poll) {
            status = MtkRead32(Spi->Registers, MTK_SPI_STATUS0);
            if ((status & (MTK_SPI_STATUS_FINISH | MTK_SPI_STATUS_PAUSE)) != 0U) {
                break;
            }
            KeStallExecutionProcessor(1);
        }
        if (poll == MTK_SPI_POLL_LIMIT) {
            MtkSpiControllerReset(Spi);
            return STATUS_IO_TIMEOUT;
        }

        if (Receive != NULL) {
            for (wordOffset = 0U; wordOffset < chunk; wordOffset += sizeof(ULONG)) {
                word = MtkRead32(Spi->Registers, MTK_SPI_RX_DATA);
                bytes = chunk - wordOffset;
                if (bytes > sizeof(ULONG)) {
                    bytes = sizeof(ULONG);
                }
                RtlCopyMemory(Receive + offset + wordOffset, &word, bytes);
            }
        }

        offset += chunk;
        paused = (status & MTK_SPI_STATUS_PAUSE) != 0U;
    }

    command = MtkRead32(Spi->Registers, MTK_SPI_CMD);
    command &= ~(MTK_SPI_CMD_PAUSE_EN | MTK_SPI_CMD_ACT | MTK_SPI_CMD_RESUME);
    MtkWrite32(Spi->Registers, MTK_SPI_CMD, command);
    MtkSpiControllerReset(Spi);
    return STATUS_SUCCESS;
}
