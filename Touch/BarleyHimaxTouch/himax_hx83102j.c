/*
 * Himax HX83102J zero-flash protocol for Lenovo TB330XU (Barley).
 *
 * The register protocol, partition-table format, CRC engine programming and
 * power-on sequence follow Himax's upstream HX83102J driver.  The firmware is
 * kept as a vendor-supplied package file rather than being compiled into the
 * source tree.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "barley_touch.h"

#define HIMAX_SPI_FUNCTION_READ             0xF3U
#define HIMAX_SPI_FUNCTION_WRITE            0xF2U
#define HIMAX_BUS_READ_HEADER                3U
#define HIMAX_BUS_WRITE_HEADER               2U
#define HIMAX_REGISTER_SIZE                  4U
#define HIMAX_REGISTER_TRANSFER_MAX          1024U
#define HIMAX_DSRAM_SIZE                     73728U
#define HIMAX_MAX_PARTITIONS                 64U
#define HIMAX_PARTITION_DESCRIPTOR_SIZE      16U
#define HIMAX_PARTITION_COUNT_OFFSET         12U

#define HIMAX_CMD_AHB_ADDRESS                0x00U
#define HIMAX_CMD_AHB_READ_DATA              0x08U
#define HIMAX_CMD_AHB_DIRECTION              0x0CU
#define HIMAX_CMD_AHB_INCREMENT              0x0DU
#define HIMAX_CMD_AHB_CONTINUOUS             0x13U
#define HIMAX_CMD_EVENT_STACK                0x30U
#define HIMAX_CMD_SAFE_MODE_LOW              0x31U
#define HIMAX_CMD_SAFE_MODE_HIGH             0x32U

#define HIMAX_AHB_DIRECTION_READ             0x00U
#define HIMAX_AHB_CONTINUOUS_VALUE           0x31U
#define HIMAX_AHB_INCREMENT_VALUE            0x10U
#define HIMAX_AHB_INCREMENT_4_VALUE          0x11U

#define HIMAX_REG_HW_CRC                     0x80010000U
#define HIMAX_REG_TCON_RESET                 0x80020004U
#define HIMAX_REG_RELOAD_STATUS              0x80050000U
#define HIMAX_REG_RELOAD_CRC_RESULT          0x80050018U
#define HIMAX_REG_RELOAD_START               0x80050020U
#define HIMAX_REG_RELOAD_LENGTH              0x80050028U
#define HIMAX_REG_SYSTEM_RESET               0x90000018U
#define HIMAX_REG_RELOAD_ACTIVE              0x90000048U
#define HIMAX_REG_CONTROL_FW                 0x9000005CU
#define HIMAX_REG_FW_STATUS                  0x900000A8U
#define HIMAX_REG_IC_ID                      0x900000D0U
#define HIMAX_REG_RESET_FLAG                 0x900000E4U

#define HIMAX_DSRAM_SORTING_MODE             0x10007F04U
#define HIMAX_DSRAM_FLASH_RELOAD             0x10007F00U
#define HIMAX_DSRAM_RAW_OUT_SELECT           0x100072ECU
#define HIMAX_DSRAM_SET_NFRAME               0x10007294U
#define HIMAX_DSRAM_SECOND_RELOAD            0x100072C0U

#define HIMAX_DATA_SYSTEM_RESET              0x00000055U
#define HIMAX_DATA_HW_CRC                    0x0000ECCEU
#define HIMAX_DATA_RELOAD_ACTIVE             0x000000ECU
#define HIMAX_DATA_RELOAD_ACTIVE_DONE        0x000001ECU
#define HIMAX_DATA_DISABLE_FLASH_RELOAD      0x00009AA9U
#define HIMAX_DATA_FW_RELOAD_DONE            0x000072C0U
#define HIMAX_DATA_FW_SAFE_MODE              0x0CU
#define HIMAX_DATA_RELOAD_PASSWORD           0x0099U
#define HIMAX_DATA_RESET_FLAG                0x00000002U
#define HIMAX_IC_ID_MASK                     0xFFFFFF00U
#define HIMAX_IC_ID_HX83102J                 0x83102900U

#define HIMAX_MAP_TOUCH_CONFIG_TABLE         0x00000A00U
#define HIMAX_BIN_HEADER_SCAN_SIZE           1024U
#define HIMAX_CRC32C_POLYNOMIAL_LE           0x82F63B78U

typedef struct _HIMAX_PARTITION_INFO {
    ULONG SramAddress;
    ULONG WriteSize;
    ULONG FirmwareOffset;
} HIMAX_PARTITION_INFO, *PHIMAX_PARTITION_INFO;

static __forceinline ULONG
HimaxReadLe32(
    _In_reads_bytes_(4) const UCHAR *Data
    )
{
    return ((ULONG)Data[0]) |
           ((ULONG)Data[1] << 8) |
           ((ULONG)Data[2] << 16) |
           ((ULONG)Data[3] << 24);
}

static __forceinline VOID
HimaxWriteLe32(
    _Out_writes_bytes_(4) UCHAR *Data,
    _In_ ULONG Value
    )
{
    Data[0] = (UCHAR)Value;
    Data[1] = (UCHAR)(Value >> 8);
    Data[2] = (UCHAR)(Value >> 16);
    Data[3] = (UCHAR)(Value >> 24);
}

static NTSTATUS
HimaxBusWrite(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ UCHAR Command,
    _In_reads_bytes_opt_(Length) const UCHAR *Data,
    _In_ ULONG Length
    )
{
    ULONG total;

    total = Length + HIMAX_BUS_WRITE_HEADER;
    if (total > sizeof(Context->SpiTxBuffer)) {
        return STATUS_BUFFER_OVERFLOW;
    }

    Context->SpiTxBuffer[0] = HIMAX_SPI_FUNCTION_WRITE;
    Context->SpiTxBuffer[1] = Command;
    if (Length != 0U && Data != NULL) {
        RtlCopyMemory(Context->SpiTxBuffer + HIMAX_BUS_WRITE_HEADER,
                      Data, Length);
    }

    return BarleyMtkSpiTransfer(&Context->Spi, Context->SpiTxBuffer,
                                NULL, total);
}

static NTSTATUS
HimaxBusRead(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ UCHAR Command,
    _Out_writes_bytes_(Length) UCHAR *Data,
    _In_ ULONG Length
    )
{
    NTSTATUS status;
    ULONG total;

    total = Length + HIMAX_BUS_READ_HEADER;
    if (Data == NULL || total > sizeof(Context->SpiTxBuffer)) {
        return STATUS_BUFFER_OVERFLOW;
    }

    RtlZeroMemory(Context->SpiTxBuffer, total);
    RtlZeroMemory(Context->SpiRxBuffer, total);
    Context->SpiTxBuffer[0] = HIMAX_SPI_FUNCTION_READ;
    Context->SpiTxBuffer[1] = Command;
    Context->SpiTxBuffer[2] = 0U;

    status = BarleyMtkSpiTransfer(&Context->Spi, Context->SpiTxBuffer,
                                  Context->SpiRxBuffer, total);
    if (NT_SUCCESS(status)) {
        RtlCopyMemory(Data, Context->SpiRxBuffer + HIMAX_BUS_READ_HEADER,
                      Length);
    }
    return status;
}

static NTSTATUS
HimaxSetBurstMode(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ BOOLEAN AutoIncrement
    )
{
    NTSTATUS status;
    UCHAR value;

    value = HIMAX_AHB_CONTINUOUS_VALUE;
    status = HimaxBusWrite(Context, HIMAX_CMD_AHB_CONTINUOUS, &value, 1U);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    value = AutoIncrement ? HIMAX_AHB_INCREMENT_4_VALUE :
                            HIMAX_AHB_INCREMENT_VALUE;
    return HimaxBusWrite(Context, HIMAX_CMD_AHB_INCREMENT, &value, 1U);
}

static NTSTATUS
HimaxRegisterWrite(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ ULONG Address,
    _In_reads_bytes_(Length) const UCHAR *Data,
    _In_ ULONG Length
    )
{
    NTSTATUS status;
    ULONG offset;
    ULONG chunk;
    UCHAR payload[HIMAX_REGISTER_TRANSFER_MAX + HIMAX_REGISTER_SIZE];

    if (Data == NULL || Length == 0U) {
        return STATUS_INVALID_PARAMETER;
    }

    status = HimaxSetBurstMode(Context,
                               (Length > HIMAX_REGISTER_SIZE) ? TRUE : FALSE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (offset = 0U; offset < Length; offset += chunk) {
        chunk = Length - offset;
        if (chunk > HIMAX_REGISTER_TRANSFER_MAX) {
            chunk = HIMAX_REGISTER_TRANSFER_MAX;
        }
        HimaxWriteLe32(payload, Address + offset);
        RtlCopyMemory(payload + HIMAX_REGISTER_SIZE, Data + offset, chunk);
        status = HimaxBusWrite(Context, HIMAX_CMD_AHB_ADDRESS, payload,
                               chunk + HIMAX_REGISTER_SIZE);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS
HimaxRegisterWrite32(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ ULONG Address,
    _In_ ULONG Value
    )
{
    UCHAR data[4];

    HimaxWriteLe32(data, Value);
    return HimaxRegisterWrite(Context, Address, data, sizeof(data));
}

static NTSTATUS
HimaxRegisterRead(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ ULONG Address,
    _Out_writes_bytes_(Length) UCHAR *Data,
    _In_ ULONG Length
    )
{
    NTSTATUS status;
    ULONG offset;
    ULONG chunk;
    UCHAR addressBytes[4];
    UCHAR direction;

    if (Data == NULL || Length == 0U) {
        return STATUS_INVALID_PARAMETER;
    }

    status = HimaxSetBurstMode(Context,
                               (Length > HIMAX_REGISTER_SIZE) ? TRUE : FALSE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (offset = 0U; offset < Length; offset += chunk) {
        chunk = Length - offset;
        if (chunk > HIMAX_REGISTER_TRANSFER_MAX) {
            chunk = HIMAX_REGISTER_TRANSFER_MAX;
        }

        HimaxWriteLe32(addressBytes, Address + offset);
        status = HimaxBusWrite(Context, HIMAX_CMD_AHB_ADDRESS,
                               addressBytes, sizeof(addressBytes));
        if (!NT_SUCCESS(status)) {
            return status;
        }

        direction = HIMAX_AHB_DIRECTION_READ;
        status = HimaxBusWrite(Context, HIMAX_CMD_AHB_DIRECTION,
                               &direction, 1U);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = HimaxBusRead(Context, HIMAX_CMD_AHB_READ_DATA,
                              Data + offset, chunk);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS
HimaxRegisterRead32(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ ULONG Address,
    _Out_ PULONG Value
    )
{
    NTSTATUS status;
    UCHAR data[4];

    if (Value == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    status = HimaxRegisterRead(Context, Address, data, sizeof(data));
    if (NT_SUCCESS(status)) {
        *Value = HimaxReadLe32(data);
    }
    return status;
}

static NTSTATUS
HimaxInterfaceOn(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    UCHAR dummy[4];
    UCHAR continuous;
    UCHAR increment;
    ULONG retry;

    status = HimaxBusRead(Context, HIMAX_CMD_AHB_READ_DATA,
                          dummy, sizeof(dummy));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (retry = 0U; retry < 10U; ++retry) {
        status = HimaxSetBurstMode(Context, FALSE);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        status = HimaxBusRead(Context, HIMAX_CMD_AHB_CONTINUOUS,
                              &continuous, 1U);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        status = HimaxBusRead(Context, HIMAX_CMD_AHB_INCREMENT,
                              &increment, 1U);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if (continuous == HIMAX_AHB_CONTINUOUS_VALUE &&
            increment == HIMAX_AHB_INCREMENT_VALUE) {
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(1000);
    }

    return STATUS_IO_DEVICE_ERROR;
}

static NTSTATUS
HimaxSenseOff(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    ULONG value;
    ULONG retry;
    UCHAR password;

    for (retry = 0U; retry < 5U; ++retry) {
        password = 0x27U;
        status = HimaxBusWrite(Context, HIMAX_CMD_SAFE_MODE_LOW,
                               &password, 1U);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        password = 0x95U;
        status = HimaxBusWrite(Context, HIMAX_CMD_SAFE_MODE_HIGH,
                               &password, 1U);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = HimaxRegisterRead32(Context, HIMAX_REG_FW_STATUS, &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if ((value & 0xFFU) == HIMAX_DATA_FW_SAFE_MODE) {
            status = HimaxRegisterWrite32(Context, HIMAX_REG_TCON_RESET, 0U);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            KeStallExecutionProcessor(1000);
            return STATUS_SUCCESS;
        }

        KeStallExecutionProcessor(5000);
        BarleyMtkSpiResetTouch(&Context->Spi);
    }

    return STATUS_IO_DEVICE_ERROR;
}

static NTSTATUS
HimaxEnableHardwareCrc(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    ULONG value;
    ULONG retry;

    for (retry = 0U; retry < 5U; ++retry) {
        status = HimaxRegisterWrite32(Context, HIMAX_REG_HW_CRC,
                                      HIMAX_DATA_HW_CRC);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        KeStallExecutionProcessor(1000);
        status = HimaxRegisterRead32(Context, HIMAX_REG_HW_CRC, &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if ((value & 0xFFFFU) == (HIMAX_DATA_HW_CRC & 0xFFFFU)) {
            return STATUS_SUCCESS;
        }
    }
    return STATUS_CRC_ERROR;
}

static NTSTATUS
HimaxReloadToActive(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    ULONG value;
    ULONG retry;

    for (retry = 0U; retry < 5U; ++retry) {
        status = HimaxRegisterWrite32(Context, HIMAX_REG_RELOAD_ACTIVE,
                                      HIMAX_DATA_RELOAD_ACTIVE);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        KeStallExecutionProcessor(1000);
        status = HimaxRegisterRead32(Context, HIMAX_REG_RELOAD_ACTIVE, &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if ((value & 0xFFFFU) == HIMAX_DATA_RELOAD_ACTIVE_DONE) {
            return STATUS_SUCCESS;
        }
    }
    return STATUS_IO_DEVICE_ERROR;
}

static NTSTATUS
HimaxSenseOn(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;

    status = HimaxInterfaceOn(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxRegisterWrite32(Context, HIMAX_REG_CONTROL_FW, 0U);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    KeStallExecutionProcessor(10000);
    BarleyMtkSpiResetTouch(&Context->Spi);

    status = HimaxEnableHardwareCrc(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    return HimaxReloadToActive(Context);
}

static NTSTATUS
HimaxDetectChip(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    ULONG value;
    ULONG retry;

    BarleyMtkSpiResetTouch(&Context->Spi);
    status = HimaxInterfaceOn(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxSenseOff(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (retry = 0U; retry < 5U; ++retry) {
        status = HimaxRegisterRead32(Context, HIMAX_REG_IC_ID, &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if ((value & HIMAX_IC_ID_MASK) == HIMAX_IC_ID_HX83102J) {
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(1000);
    }
    return STATUS_DEVICE_DOES_NOT_EXIST;
}

static NTSTATUS
HimaxLoadFirmwareFile(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    HANDLE file;
    UNICODE_STRING path;
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK ioStatus;
    FILE_STANDARD_INFORMATION information;
    LARGE_INTEGER offset;

    if (Context->Firmware != NULL) {
        return STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&path, BARLEY_TOUCH_FIRMWARE_NAME);
    InitializeObjectAttributes(&attributes, &path,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    file = NULL;
    status = ZwCreateFile(&file, GENERIC_READ | SYNCHRONIZE, &attributes,
                          &ioStatus, NULL, FILE_ATTRIBUTE_NORMAL,
                          FILE_SHARE_READ, FILE_OPEN,
                          FILE_NON_DIRECTORY_FILE |
                          FILE_SYNCHRONOUS_IO_NONALERT,
                          NULL, 0U);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = ZwQueryInformationFile(file, &ioStatus, &information,
                                    sizeof(information),
                                    FileStandardInformation);
    if (!NT_SUCCESS(status)) {
        ZwClose(file);
        return status;
    }
    if (information.EndOfFile.HighPart != 0 ||
        information.EndOfFile.LowPart != BARLEY_TOUCH_FIRMWARE_SIZE) {
        ZwClose(file);
        return STATUS_FILE_CORRUPT_ERROR;
    }

    Context->Firmware = (PUCHAR)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, BARLEY_TOUCH_FIRMWARE_SIZE,
        BARLEY_TOUCH_POOL_TAG);
    if (Context->Firmware == NULL) {
        ZwClose(file);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    Context->FirmwareLength = BARLEY_TOUCH_FIRMWARE_SIZE;
    offset.QuadPart = 0;
    status = ZwReadFile(file, NULL, NULL, NULL, &ioStatus,
                        Context->Firmware, Context->FirmwareLength,
                        &offset, NULL);
    ZwClose(file);
    if (!NT_SUCCESS(status) ||
        ioStatus.Information != Context->FirmwareLength) {
        BarleyHimaxReleaseFirmware(Context);
        return NT_SUCCESS(status) ? STATUS_END_OF_FILE : status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS
HimaxFindConfigurationTable(
    _In_reads_bytes_(FirmwareLength) const UCHAR *Firmware,
    _In_ ULONG FirmwareLength,
    _Out_ PULONG TableOffset
    )
{
    ULONG page;
    ULONG token;
    ULONG index;
    ULONG sum;
    ULONG nonZero;
    ULONG mapCode;
    ULONG imageOffset;

    if (Firmware == NULL || TableOffset == NULL ||
        FirmwareLength < HIMAX_BIN_HEADER_SCAN_SIZE ||
        Firmware[14] != 0x87U) {
        return STATUS_FILE_CORRUPT_ERROR;
    }
    for (index = 0U; index < 8U; ++index) {
        if (Firmware[index] != 0U) {
            return STATUS_FILE_CORRUPT_ERROR;
        }
    }

    for (page = 0U; page < HIMAX_BIN_HEADER_SCAN_SIZE; page += 128U) {
        for (token = page; token < page + 128U; token += 16U) {
            sum = 0U;
            nonZero = 0U;
            for (index = 0U; index < 16U; ++index) {
                sum += Firmware[token + index];
                nonZero |= Firmware[token + index];
            }
            if (nonZero == 0U) {
                break;
            }
            if ((sum & 0xFFU) != 0U) {
                continue;
            }
            mapCode = HimaxReadLe32(Firmware + token);
            imageOffset = HimaxReadLe32(Firmware + token + 4U);
            if (mapCode == HIMAX_MAP_TOUCH_CONFIG_TABLE) {
                if (imageOffset >= FirmwareLength) {
                    return STATUS_FILE_CORRUPT_ERROR;
                }
                *TableOffset = imageOffset;
                return STATUS_SUCCESS;
            }
        }
    }
    return STATUS_NOT_FOUND;
}

static ULONG
HimaxCalculateCrc32c(
    _In_reads_bytes_(Length) const UCHAR *Data,
    _In_ ULONG Length
    )
{
    ULONG crc;
    ULONG current;
    ULONG index;
    ULONG bit;

    crc = MAXULONG;
    for (index = 0U; index + 4U <= Length; index += 4U) {
        current = HimaxReadLe32(Data + index);
        crc ^= current;
        for (bit = 0U; bit < 32U; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ HIMAX_CRC32C_POLYNOMIAL_LE;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static NTSTATUS
HimaxCheckHardwareCrc(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ ULONG StartAddress,
    _In_ ULONG Length,
    _Out_ PULONG Crc
    )
{
    NTSTATUS status;
    ULONG value;
    ULONG retry;
    ULONG wordLength;

    if (Crc == NULL || (Length & 3U) != 0U) {
        return STATUS_INVALID_PARAMETER;
    }

    status = HimaxRegisterWrite32(Context, HIMAX_REG_RELOAD_START,
                                  StartAddress);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    wordLength = Length / 4U;
    value = (HIMAX_DATA_RELOAD_PASSWORD << 16) | (wordLength & 0xFFFFU);
    status = HimaxRegisterWrite32(Context, HIMAX_REG_RELOAD_LENGTH, value);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxRegisterRead32(Context, HIMAX_REG_RELOAD_LENGTH, &value);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if ((value & 0xFFFFU) != wordLength) {
        return STATUS_DATA_ERROR;
    }

    for (retry = 0U; retry < 100U; ++retry) {
        status = HimaxRegisterRead32(Context, HIMAX_REG_RELOAD_STATUS,
                                     &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if ((value & 1U) == 0U) {
            return HimaxRegisterRead32(Context,
                                       HIMAX_REG_RELOAD_CRC_RESULT, Crc);
        }
        KeStallExecutionProcessor(1000);
    }
    return STATUS_IO_TIMEOUT;
}

static NTSTATUS
HimaxWriteWithCrc(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _In_ ULONG Address,
    _In_reads_bytes_(Length) const UCHAR *Data,
    _In_ ULONG Length,
    _In_ ULONG ExpectedCrc
    )
{
    NTSTATUS status;
    ULONG crc;
    ULONG retry;

    for (retry = 0U; retry < 3U; ++retry) {
        status = HimaxRegisterWrite(Context, Address, Data, Length);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        status = HimaxCheckHardwareCrc(Context, Address, Length, &crc);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if (crc == ExpectedCrc) {
            return STATUS_SUCCESS;
        }
    }
    return STATUS_CRC_ERROR;
}

static NTSTATUS
HimaxUploadPartitions(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    HIMAX_PARTITION_INFO partitions[HIMAX_MAX_PARTITIONS];
    ULONG table;
    ULONG count;
    ULONG index;
    ULONG remainder;
    ULONG dsramBase;
    ULONG dsramEnd;
    ULONG configLength;
    ULONG configCrc;

    status = HimaxFindConfigurationTable(Context->Firmware,
                                          Context->FirmwareLength, &table);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    if (table + HIMAX_PARTITION_COUNT_OFFSET >= Context->FirmwareLength) {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    count = Context->Firmware[table + HIMAX_PARTITION_COUNT_OFFSET];
    if (count < 2U || count > HIMAX_MAX_PARTITIONS ||
        table + count * HIMAX_PARTITION_DESCRIPTOR_SIZE >
        Context->FirmwareLength) {
        return STATUS_FILE_CORRUPT_ERROR;
    }
    RtlZeroMemory(partitions, sizeof(partitions));

    dsramBase = MAXULONG;
    dsramEnd = 0U;
    for (index = 0U; index < count; ++index) {
        const UCHAR *descriptor;

        descriptor = Context->Firmware + table +
                     index * HIMAX_PARTITION_DESCRIPTOR_SIZE;
        partitions[index].SramAddress = HimaxReadLe32(descriptor);
        partitions[index].WriteSize = HimaxReadLe32(descriptor + 4U);
        partitions[index].FirmwareOffset = HimaxReadLe32(descriptor + 8U);

        if (partitions[index].WriteSize == 0U ||
            partitions[index].FirmwareOffset > Context->FirmwareLength ||
            partitions[index].WriteSize > Context->FirmwareLength -
                                                  partitions[index].FirmwareOffset) {
            return STATUS_FILE_CORRUPT_ERROR;
        }

        if (index == 0U) {
            continue;
        }
        remainder = partitions[index].SramAddress & 3U;
        if (remainder != 0U) {
            if (partitions[index].FirmwareOffset < remainder) {
                return STATUS_FILE_CORRUPT_ERROR;
            }
            partitions[index].SramAddress -= remainder;
            partitions[index].FirmwareOffset -= remainder;
            partitions[index].WriteSize += remainder;
        }
        if (partitions[index].SramAddress < dsramBase) {
            dsramBase = partitions[index].SramAddress;
        }
        if (partitions[index].SramAddress + partitions[index].WriteSize >
            dsramEnd) {
            dsramEnd = partitions[index].SramAddress +
                       partitions[index].WriteSize;
        }
    }

    if (dsramBase == MAXULONG || dsramEnd <= dsramBase) {
        return STATUS_FILE_CORRUPT_ERROR;
    }
    configLength = (dsramEnd - dsramBase + 3U) & ~3U;
    if (configLength > HIMAX_DSRAM_SIZE) {
        return STATUS_BUFFER_OVERFLOW;
    }

    Context->ConfigBuffer = (PUCHAR)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, HIMAX_DSRAM_SIZE, BARLEY_TOUCH_POOL_TAG);
    if (Context->ConfigBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    Context->ConfigBufferLength = HIMAX_DSRAM_SIZE;
    RtlZeroMemory(Context->ConfigBuffer, HIMAX_DSRAM_SIZE);

    for (index = 1U; index < count; ++index) {
        ULONG destination;

        destination = partitions[index].SramAddress - dsramBase;
        if (destination > configLength ||
            partitions[index].WriteSize > configLength - destination) {
            return STATUS_FILE_CORRUPT_ERROR;
        }
        RtlCopyMemory(Context->ConfigBuffer + destination,
                      Context->Firmware + partitions[index].FirmwareOffset,
                      partitions[index].WriteSize);
    }

    status = HimaxEnableHardwareCrc(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxWriteWithCrc(
        Context, partitions[0].SramAddress,
        Context->Firmware + partitions[0].FirmwareOffset,
        partitions[0].WriteSize, 0U);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    configCrc = HimaxCalculateCrc32c(Context->ConfigBuffer, configLength);
    return HimaxWriteWithCrc(Context, dsramBase, Context->ConfigBuffer,
                             configLength, configCrc);
}

static NTSTATUS
HimaxPowerOnInitialize(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    ULONG value;
    ULONG retry;

    status = HimaxRegisterWrite32(Context, HIMAX_DSRAM_RAW_OUT_SELECT, 0U);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxRegisterWrite32(Context, HIMAX_DSRAM_SORTING_MODE, 0U);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxRegisterRead32(Context, HIMAX_DSRAM_SORTING_MODE, &value);
    if (!NT_SUCCESS(status) || value != 0U) {
        return NT_SUCCESS(status) ? STATUS_DATA_ERROR : status;
    }
    status = HimaxRegisterWrite32(Context, HIMAX_DSRAM_SET_NFRAME, 1U);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = HimaxRegisterWrite32(Context, HIMAX_DSRAM_SECOND_RELOAD, 0U);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = HimaxSenseOn(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    for (retry = 0U; retry < 30U; ++retry) {
        status = HimaxRegisterRead32(Context, HIMAX_REG_RESET_FLAG, &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if (value != HIMAX_DATA_RESET_FLAG) {
            return STATUS_RETRY;
        }
        status = HimaxRegisterRead32(Context, HIMAX_DSRAM_SECOND_RELOAD,
                                     &value);
        if (!NT_SUCCESS(status)) {
            return status;
        }
        if (value == HIMAX_DATA_FW_RELOAD_DONE) {
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(10000);
    }
    return STATUS_IO_TIMEOUT;
}

NTSTATUS
BarleyHimaxInitialize(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;

    if (Context == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Context->InitializationStage = 1U;
    status = HimaxLoadFirmwareFile(Context);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    Context->InitializationStage = 2U;
    status = HimaxDetectChip(Context);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    Context->InitializationStage = 3U;
    status = HimaxRegisterWrite32(Context, HIMAX_REG_SYSTEM_RESET,
                                  HIMAX_DATA_SYSTEM_RESET);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    status = HimaxSenseOff(Context);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    Context->InitializationStage = 4U;
    status = HimaxUploadPartitions(Context);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    Context->InitializationStage = 5U;
    status = HimaxRegisterWrite32(Context, HIMAX_DSRAM_FLASH_RELOAD,
                                  HIMAX_DATA_DISABLE_FLASH_RELOAD);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    Context->InitializationStage = 6U;
    status = HimaxPowerOnInitialize(Context);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    Context->InitializationStage = 7U;

Exit:
    Context->InitializationStatus = status;
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "BarleyHimaxTouch: init stage %lu status 0x%08X\n",
        Context->InitializationStage, status));
    return status;
}

NTSTATUS
BarleyHimaxReadTouchReport(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _Out_ PBARLEY_TOUCH_INPUT_REPORT Report
    )
{
    NTSTATUS status;
    ULONG index;
    ULONG checksum;
    ULONG zeroCount;
    ULONG reportedCount;
    ULONG outputIndex;
    ULONG previousIndex;
    USHORT x;
    USHORT y;
    UCHAR validCount;
    BOOLEAN active[BARLEY_TOUCH_MAX_CONTACTS];

    if (Context == NULL || Report == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    status = HimaxBusRead(Context, HIMAX_CMD_EVENT_STACK,
                          Context->EventBuffer, BARLEY_TOUCH_EVENT_SIZE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    checksum = 0U;
    zeroCount = 0U;
    for (index = 0U; index < BARLEY_TOUCH_EVENT_SIZE; ++index) {
        checksum += Context->EventBuffer[index];
        if (Context->EventBuffer[index] == 0U) {
            ++zeroCount;
        }
    }
    if ((checksum & 0xFFU) != 0U || zeroCount == BARLEY_TOUCH_EVENT_SIZE) {
        return STATUS_DATA_ERROR;
    }

    reportedCount = (Context->EventBuffer[52] == 0xFFU) ? 0U :
                    (Context->EventBuffer[52] & 0x0FU);
    if (reportedCount > BARLEY_TOUCH_MAX_CONTACTS) {
        return STATUS_DATA_ERROR;
    }

    RtlZeroMemory(Report, sizeof(*Report));
    RtlZeroMemory(active, sizeof(active));
    Report->ReportId = BARLEY_TOUCH_REPORT_ID;
    validCount = 0U;
    outputIndex = 0U;
    for (index = 0U; index < BARLEY_TOUCH_MAX_CONTACTS; ++index) {
        x = (USHORT)(((USHORT)Context->EventBuffer[index * 4U] << 8) |
                      Context->EventBuffer[index * 4U + 1U]);
        y = (USHORT)(((USHORT)Context->EventBuffer[index * 4U + 2U] << 8) |
                      Context->EventBuffer[index * 4U + 3U]);

        if (validCount < reportedCount && x <= BARLEY_TOUCH_X_MAX &&
            y <= BARLEY_TOUCH_Y_MAX) {
            active[index] = TRUE;
            Report->Contacts[outputIndex].ContactId = (UCHAR)index;
            Report->Contacts[outputIndex].X = x;
            Report->Contacts[outputIndex].Y = y;
            Report->Contacts[outputIndex].Flags =
                BARLEY_TOUCH_FLAG_TIP_SWITCH |
                BARLEY_TOUCH_FLAG_IN_RANGE |
                BARLEY_TOUCH_FLAG_CONFIDENCE;
            ++validCount;
            ++outputIndex;
        }
    }

    /*
     * A parallel Windows touch report must explicitly retire a departed
     * contact.  Preserve its identifier and last coordinates for one frame,
     * with Tip Switch and In Range clear.  LastReport changes only after VHF
     * accepts a report, so a transient VHF back-pressure failure retries the
     * same liftoff on the next sample.
     */
    if (Context->HaveLastReport) {
        for (previousIndex = 0U;
             previousIndex < Context->LastReport.ContactCount &&
             outputIndex < BARLEY_TOUCH_MAX_CONTACTS;
             ++previousIndex) {
            PBARLEY_TOUCH_CONTACT_REPORT previous;

            previous = &Context->LastReport.Contacts[previousIndex];
            if ((previous->Flags & BARLEY_TOUCH_FLAG_TIP_SWITCH) == 0U ||
                previous->ContactId >= BARLEY_TOUCH_MAX_CONTACTS ||
                active[previous->ContactId]) {
                continue;
            }

            Report->Contacts[outputIndex].Flags =
                BARLEY_TOUCH_FLAG_CONFIDENCE;
            Report->Contacts[outputIndex].ContactId = previous->ContactId;
            Report->Contacts[outputIndex].X = previous->X;
            Report->Contacts[outputIndex].Y = previous->Y;
            ++outputIndex;
        }
    }

    Report->ContactCount = (UCHAR)outputIndex;
    return STATUS_SUCCESS;
}

VOID
BarleyHimaxReleaseFirmware(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    if (Context == NULL) {
        return;
    }
    if (Context->ConfigBuffer != NULL) {
        ExFreePoolWithTag(Context->ConfigBuffer, BARLEY_TOUCH_POOL_TAG);
        Context->ConfigBuffer = NULL;
        Context->ConfigBufferLength = 0U;
    }
    if (Context->Firmware != NULL) {
        ExFreePoolWithTag(Context->Firmware, BARLEY_TOUCH_POOL_TAG);
        Context->Firmware = NULL;
        Context->FirmwareLength = 0U;
    }
}
