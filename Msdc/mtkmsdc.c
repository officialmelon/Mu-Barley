#include "mtkmsdc.h"

#define MTK_MSDC_CONTROL_TIMEOUT_US  100000UL
#define MTK_MSDC_COMMAND_TIMEOUT_US 1000000UL
#define MTK_MSDC_DATA_TIMEOUT_US    5000000UL
#define MTK_MSDC_BUSY_TIMEOUT_US      20000UL

#define MTK_MSDC_LOG(_level, _format, ...)                              \
    KdPrintEx((DPFLTR_IHVDRIVER_ID, (_level),                           \
               "mtkmsdc: " _format, __VA_ARGS__))

static __forceinline ULONG
MtkMsdcRead(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG Offset
    )
{
    return SdPortReadRegisterUlong(Extension->BaseAddress, Offset);
}

static __forceinline VOID
MtkMsdcWrite(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG Offset,
    _In_ ULONG Value
    )
{
    SdPortWriteRegisterUlong(Extension->BaseAddress, Offset, Value);
}

static __forceinline VOID
MtkMsdcSetBits(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG Offset,
    _In_ ULONG Bits
    )
{
    MtkMsdcWrite(Extension, Offset, MtkMsdcRead(Extension, Offset) | Bits);
}

static __forceinline VOID
MtkMsdcClearBits(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG Offset,
    _In_ ULONG Bits
    )
{
    MtkMsdcWrite(Extension, Offset, MtkMsdcRead(Extension, Offset) & ~Bits);
}

static NTSTATUS
MtkMsdcWaitClear(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG Offset,
    _In_ ULONG Mask,
    _In_ ULONG TimeoutMicroseconds
    )
{
    ULONG Poll;

    for (Poll = 0; Poll < TimeoutMicroseconds; Poll += 1) {
        if ((MtkMsdcRead(Extension, Offset) & Mask) == 0) {
            return STATUS_SUCCESS;
        }

        SdPortWait(1);
    }

    return STATUS_IO_TIMEOUT;
}

static NTSTATUS
MtkMsdcWaitSet(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG Offset,
    _In_ ULONG Mask,
    _In_ ULONG TimeoutMicroseconds
    )
{
    ULONG Poll;

    for (Poll = 0; Poll < TimeoutMicroseconds; Poll += 1) {
        if ((MtkMsdcRead(Extension, Offset) & Mask) == Mask) {
            return STATUS_SUCCESS;
        }

        SdPortWait(1);
    }

    return STATUS_IO_TIMEOUT;
}

static NTSTATUS
MtkMsdcResetController(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    MtkMsdcSetBits(Extension, MSDC_CFG, MSDC_CFG_RST);
    return MtkMsdcWaitClear(Extension,
                            MSDC_CFG,
                            MSDC_CFG_RST,
                            MTK_MSDC_CONTROL_TIMEOUT_US);
}

static NTSTATUS
MtkMsdcClearFifo(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    MtkMsdcSetBits(Extension, MSDC_FIFOCS, MSDC_FIFOCS_CLR);
    return MtkMsdcWaitClear(Extension,
                            MSDC_FIFOCS,
                            MSDC_FIFOCS_CLR,
                            MTK_MSDC_CONTROL_TIMEOUT_US);
}

static VOID
MtkMsdcClearAllInterrupts(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    ULONG Status;

    Status = MtkMsdcRead(Extension, MSDC_INT);
    if (Status != 0) {
        MtkMsdcWrite(Extension, MSDC_INT, Status);
    }
}

static NTSTATUS
MtkMsdcRecover(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    NTSTATUS Status;

    Status = MtkMsdcResetController(Extension);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = MtkMsdcClearFifo(Extension);
    MtkMsdcClearAllInterrupts(Extension);
    return Status;
}

static NTSTATUS
MtkMsdcStatusFromInterrupt(
    _In_ ULONG InterruptStatus
    )
{
    if ((InterruptStatus & (MSDC_INT_CMDTMO |
                            MSDC_INT_ACMDTMO |
                            MSDC_INT_DATTMO)) != 0) {
        return STATUS_IO_TIMEOUT;
    }

    if ((InterruptStatus & (MSDC_INT_CMDCRCERR |
                            MSDC_INT_ACMDCRCERR |
                            MSDC_INT_DATCRCERR)) != 0) {
        return STATUS_CRC_ERROR;
    }

    return STATUS_IO_DEVICE_ERROR;
}

static ULONG
MtkMsdcErrorsFromInterrupt(
    _In_ ULONG InterruptStatus
    )
{
    ULONG Errors;

    Errors = 0;
    if ((InterruptStatus & MSDC_INT_CMDTMO) != 0) {
        Errors |= SDPORT_ERROR_CMD_TIMEOUT;
    }
    if ((InterruptStatus & MSDC_INT_CMDCRCERR) != 0) {
        Errors |= SDPORT_ERROR_CMD_CRC_ERROR;
    }
    if ((InterruptStatus & MSDC_INT_DATTMO) != 0) {
        Errors |= SDPORT_ERROR_DATA_TIMEOUT;
    }
    if ((InterruptStatus & MSDC_INT_DATCRCERR) != 0) {
        Errors |= SDPORT_ERROR_DATA_CRC_ERROR;
    }
    if ((InterruptStatus & MSDC_INT_ACMDTMO) != 0) {
        Errors |= SDPORT_ERROR_AUTO_CMD12_ERROR |
                  SDPORT_ERROR_ACMD12_RESPONSE_TIMEOUT;
    }
    if ((InterruptStatus & MSDC_INT_ACMDCRCERR) != 0) {
        Errors |= SDPORT_ERROR_AUTO_CMD12_ERROR |
                  SDPORT_ERROR_ACMD12_RESPONSE_CRC_ERROR;
    }
    if ((InterruptStatus & (MSDC_INT_BDCSERR | MSDC_INT_GPDCSERR)) != 0) {
        Errors |= SDPORT_ERROR_ADMA_ERROR;
    }

    return Errors;
}

static NTSTATUS
MtkMsdcWaitReady(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ BOOLEAN WaitForData
    )
{
    ULONG Mask;

    Mask = SDC_STS_CMDBUSY;
    if (WaitForData != FALSE) {
        Mask |= SDC_STS_SDCBUSY;
    }

    return MtkMsdcWaitClear(Extension,
                            SDC_STS,
                            Mask,
                            MTK_MSDC_BUSY_TIMEOUT_US);
}

static NTSTATUS
MtkMsdcSetClock(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ ULONG RequestedHz
    )
{
    ULONG ActualHz;
    ULONG Config;
    ULONG Divider;
    ULONG Mode;

    if (RequestedHz == 0) {
        MtkMsdcClearBits(Extension, MSDC_CFG, MSDC_CFG_CKPD);
        Extension->CurrentClockHz = 0;
        return STATUS_SUCCESS;
    }

    if (RequestedHz > MTK_MSDC_MAX_CLOCK_HZ) {
        RequestedHz = MTK_MSDC_MAX_CLOCK_HZ;
    }

    if (RequestedHz >= MTK_MSDC_SOURCE_CLOCK_HZ) {
        Divider = 0;
        Mode = 1;
        ActualHz = MTK_MSDC_SOURCE_CLOCK_HZ;
    } else if (RequestedHz >= (MTK_MSDC_SOURCE_CLOCK_HZ >> 1)) {
        Divider = 0;
        Mode = 0;
        ActualHz = MTK_MSDC_SOURCE_CLOCK_HZ >> 1;
    } else {
        Divider = (MTK_MSDC_SOURCE_CLOCK_HZ +
                   ((RequestedHz << 2) - 1)) / (RequestedHz << 2);
        if (Divider == 0 || Divider > 0xfff) {
            return STATUS_INVALID_PARAMETER;
        }

        Mode = 0;
        ActualHz = (MTK_MSDC_SOURCE_CLOCK_HZ >> 2) / Divider;
    }

    Config = MtkMsdcRead(Extension, MSDC_CFG);
    Config &= ~(MSDC_CFG_CKMOD_MASK |
                MSDC_CFG_CKDIV_MASK |
                MSDC_CFG_HS400_CK_MODE |
                MSDC_CFG_CKPD);
    Config |= (Mode << 20) | (Divider << 8);

    MtkMsdcClearBits(Extension, MSDC_CFG, MSDC_CFG_CKPD);
    MtkMsdcWrite(Extension, MSDC_CFG, Config);
    MtkMsdcSetBits(Extension, MSDC_CFG, MSDC_CFG_CKPD);

    if (!NT_SUCCESS(MtkMsdcWaitSet(Extension,
                                    MSDC_CFG,
                                    MSDC_CFG_CKSTB,
                                    MTK_MSDC_CONTROL_TIMEOUT_US))) {
        MtkMsdcClearBits(Extension, MSDC_CFG, MSDC_CFG_CKPD);
        return STATUS_IO_TIMEOUT;
    }

    Extension->CurrentClockHz = ActualHz;
    return STATUS_SUCCESS;
}

static NTSTATUS
MtkMsdcSetBusWidth(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_ SDPORT_BUS_WIDTH Width
    )
{
    ULONG EncodedWidth;
    ULONG Value;

    switch (Width) {
    case SdBusWidth1Bit:
        EncodedWidth = MSDC_BUS_WIDTH_1;
        break;
    case SdBusWidth4Bit:
        EncodedWidth = MSDC_BUS_WIDTH_4;
        break;
    default:
        return STATUS_NOT_SUPPORTED;
    }

    Value = MtkMsdcRead(Extension, SDC_CFG);
    Value &= ~SDC_CFG_BUS_WIDTH_MASK;
    Value |= EncodedWidth << SDC_CFG_BUS_WIDTH_SHIFT;
    MtkMsdcWrite(Extension, SDC_CFG, Value);
    return STATUS_SUCCESS;
}

static NTSTATUS
MtkMsdcResponseEncoding(
    _In_ SDPORT_RESPONSE_TYPE ResponseType,
    _Out_ PULONG Encoding
    )
{
    switch (ResponseType) {
    case SdResponseTypeNone:
        *Encoding = 0;
        return STATUS_SUCCESS;
    case SdResponseTypeR1:
    case SdResponseTypeR5:
    case SdResponseTypeR6:
        *Encoding = 1;
        return STATUS_SUCCESS;
    case SdResponseTypeR2:
        *Encoding = 2;
        return STATUS_SUCCESS;
    case SdResponseTypeR3:
        *Encoding = 3;
        return STATUS_SUCCESS;
    case SdResponseTypeR4:
        *Encoding = 4;
        return STATUS_SUCCESS;
    case SdResponseTypeR1B:
    case SdResponseTypeR5B:
        *Encoding = 7;
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

static NTSTATUS
MtkMsdcPollCommand(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    ULONG Observed;
    ULONG Poll;

    for (Poll = 0; Poll < MTK_MSDC_COMMAND_TIMEOUT_US; Poll += 1) {
        Observed = MtkMsdcRead(Extension, MSDC_INT) & MSDC_INT_CMD_STATUS;
        if (Observed != 0) {
            MtkMsdcWrite(Extension, MSDC_INT, Observed);
            if ((Observed & MSDC_INT_CMD_ERROR) != 0) {
                return MtkMsdcStatusFromInterrupt(Observed);
            }
            if ((Observed & MSDC_INT_CMDRDY) != 0) {
                return STATUS_SUCCESS;
            }
        }

        SdPortWait(1);
    }

    return STATUS_IO_TIMEOUT;
}

static VOID
MtkMsdcCaptureResponse(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    Extension->Response[0] = MtkMsdcRead(Extension, SDC_RESP0);
    Extension->Response[1] = MtkMsdcRead(Extension, SDC_RESP1);
    Extension->Response[2] = MtkMsdcRead(Extension, SDC_RESP2);
    Extension->Response[3] = MtkMsdcRead(Extension, SDC_RESP3);
}

static NTSTATUS
MtkMsdcIssueCommand(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _Inout_ PSDPORT_REQUEST Request
    )
{
    PSDPORT_COMMAND Command;
    BOOLEAN HasData;
    BOOLEAN WaitForData;
    NTSTATUS Status;
    ULONG BlockCount;
    ULONG RawCommand;
    ULONG ResponseEncoding;
    ULONG RxCount;
    ULONG TxCount;

    Command = &Request->Command;
    if (Command->Index > 63) {
        return STATUS_INVALID_PARAMETER;
    }

    HasData = (Command->TransferType != SdTransferTypeNone &&
               Command->TransferType != SdTransferTypeUndefined);
    if (HasData && Command->TransferDirection == SdTransferDirectionWrite) {
        return STATUS_MEDIA_WRITE_PROTECTED;
    }

    Status = MtkMsdcResponseEncoding(Command->ResponseType,
                                     &ResponseEncoding);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    RawCommand = Command->Index |
                 (ResponseEncoding << SDC_CMD_RSP_TYPE_SHIFT);
    if (Command->Type == SdCommandTypeAbort) {
        RawCommand |= SDC_CMD_STOP_CMD;
    }

    if (HasData) {
        if (Command->BlockSize == 0 || Command->BlockSize > 0xfff) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        BlockCount = Command->BlockCount;
        if (BlockCount == 0) {
            BlockCount = 1;
        }
        if (BlockCount > 1) {
            return STATUS_NOT_SUPPORTED;
        }

        switch (Command->TransferType) {
        case SdTransferTypeSingleBlock:
            RawCommand |= SDC_CMD_SINGLE_BLK;
            break;
        case SdTransferTypeMultiBlock:
        case SdTransferTypeMultiBlockNoStop:
            RawCommand |= SDC_CMD_MULTIPLE_BLK;
            if (Command->UseAutoCmd12 != FALSE) {
                RawCommand |= SDC_CMD_AUTO12;
            }
            break;
        default:
            return STATUS_INVALID_PARAMETER;
        }

        RawCommand |= ((ULONG)Command->BlockSize << SDC_CMD_BLK_SIZE_SHIFT);
        MtkMsdcWrite(Extension, SDC_BLK_NUM, BlockCount);
    }

    WaitForData = HasData ||
                  Command->ResponseType == SdResponseTypeR1B ||
                  Command->ResponseType == SdResponseTypeR5B;
    Status = MtkMsdcWaitReady(Extension, WaitForData);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    RxCount = MtkMsdcRead(Extension, MSDC_FIFOCS) &
              MSDC_FIFOCS_RXCNT_MASK;
    TxCount = (MtkMsdcRead(Extension, MSDC_FIFOCS) &
               MSDC_FIFOCS_TXCNT_MASK) >> MSDC_FIFOCS_TXCNT_SHIFT;
    if (RxCount != 0 || TxCount != 0) {
        Status = MtkMsdcRecover(Extension);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    /* This first version completes requests synchronously by polling. */
    MtkMsdcWrite(Extension, MSDC_INTEN, 0);
    MtkMsdcClearAllInterrupts(Extension);
    MtkMsdcWrite(Extension, SDC_ARG, Command->Argument);
    MtkMsdcWrite(Extension, SDC_CMD, RawCommand);

    Status = MtkMsdcPollCommand(Extension);
    if (!NT_SUCCESS(Status)) {
        (VOID)MtkMsdcRecover(Extension);
        return Status;
    }

    MtkMsdcCaptureResponse(Extension);
    if (!HasData &&
        (Command->ResponseType == SdResponseTypeR1B ||
         Command->ResponseType == SdResponseTypeR5B)) {
        Status = MtkMsdcWaitReady(Extension, TRUE);
    }

    Request->RequiredEvents = 0;
    Request->Status = Status;
    return Status;
}

static VOID
MtkMsdcReadFifo(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _Out_writes_bytes_(Length) PUCHAR Buffer,
    _In_ ULONG Length
    )
{
    ULONG Value;

    while (Length >= sizeof(Value)) {
        Value = SdPortReadRegisterUlong(Extension->BaseAddress, MSDC_RXDATA);
        RtlCopyMemory(Buffer, &Value, sizeof(Value));
        Buffer += sizeof(Value);
        Length -= sizeof(Value);
    }

    while (Length != 0) {
        *Buffer = SdPortReadRegisterUchar(Extension->BaseAddress, MSDC_RXDATA);
        Buffer += 1;
        Length -= 1;
    }
}

static NTSTATUS
MtkMsdcPioRead(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _Out_writes_bytes_(Length) PUCHAR Buffer,
    _In_ ULONG Length
    )
{
    BOOLEAN TransferComplete;
    ULONG Chunk;
    ULONG InterruptStatus;
    ULONG Observed;
    ULONG Poll;
    ULONG Remaining;
    ULONG RxCount;

    if (Buffer == NULL || Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    Remaining = Length;
    TransferComplete = FALSE;
    InterruptStatus = 0;

    for (Poll = 0; Poll < MTK_MSDC_DATA_TIMEOUT_US; Poll += 1) {
        InterruptStatus = MtkMsdcRead(Extension, MSDC_INT);
        Observed = InterruptStatus & MSDC_INT_DATA_STATUS;

        RxCount = MtkMsdcRead(Extension, MSDC_FIFOCS) &
                  MSDC_FIFOCS_RXCNT_MASK;
        Chunk = Remaining > MSDC_FIFO_SIZE ? MSDC_FIFO_SIZE : Remaining;
        if (Chunk != 0 && RxCount >= Chunk) {
            MtkMsdcReadFifo(Extension, Buffer, Chunk);
            Buffer += Chunk;
            Remaining -= Chunk;
        }

        if (Observed != 0) {
            MtkMsdcWrite(Extension, MSDC_INT, Observed);
        }
        if ((Observed & (MSDC_INT_DATA_ERROR | MSDC_INT_ACMD_ERROR)) != 0) {
            return MtkMsdcStatusFromInterrupt(Observed);
        }
        if ((Observed & MSDC_INT_XFER_COMPL) != 0) {
            TransferComplete = TRUE;
        }
        if (TransferComplete && Remaining == 0) {
            return STATUS_SUCCESS;
        }

        SdPortWait(1);
    }

    MTK_MSDC_LOG(DPFLTR_ERROR_LEVEL,
                 "PIO read timed out, remaining=%lu int=%08lx\n",
                 Remaining,
                 InterruptStatus);
    return STATUS_IO_TIMEOUT;
}

static NTSTATUS
MtkMsdcStartTransfer(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _Inout_ PSDPORT_REQUEST Request
    )
{
    NTSTATUS Status;
    ULONG Length;

    if (Request->Command.TransferMethod != SdTransferMethodPio) {
        return STATUS_NOT_SUPPORTED;
    }
    if (Request->Command.TransferDirection == SdTransferDirectionWrite) {
        return STATUS_MEDIA_WRITE_PROTECTED;
    }
    if (Request->Command.TransferDirection != SdTransferDirectionRead) {
        return STATUS_INVALID_PARAMETER;
    }

    Length = Request->Command.BlockSize;
    if (Length == 0 || Length > Request->Command.Length) {
        Length = Request->Command.Length;
    }
    if (Request->Command.BlockCount > 1) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = MtkMsdcPioRead(Extension,
                            Request->Command.DataBuffer,
                            Length);
    if (!NT_SUCCESS(Status)) {
        (VOID)MtkMsdcRecover(Extension);
    }

    Request->RequiredEvents = 0;
    Request->Status = Status;
    return Status;
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

_Use_decl_annotations_
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    SDPORT_INITIALIZATION_DATA InitializationData;

    RtlZeroMemory(&InitializationData, sizeof(InitializationData));
    InitializationData.StructureSize = sizeof(InitializationData);
    InitializationData.GetSlotCount = MtkMsdcGetSlotCount;
    InitializationData.GetSlotCapabilities = MtkMsdcGetSlotCapabilities;
    InitializationData.Initialize = MtkMsdcInitialize;
    InitializationData.IssueBusOperation = MtkMsdcIssueBusOperation;
    InitializationData.GetCardDetectState = MtkMsdcGetCardDetectState;
    InitializationData.GetWriteProtectState = MtkMsdcGetWriteProtectState;
    InitializationData.Interrupt = MtkMsdcInterrupt;
    InitializationData.IssueRequest = MtkMsdcIssueRequest;
    InitializationData.GetResponse = MtkMsdcGetResponse;
    InitializationData.RequestDpc = MtkMsdcRequestDpc;
    InitializationData.ToggleEvents = MtkMsdcToggleEvents;
    InitializationData.ClearEvents = MtkMsdcClearEvents;
    InitializationData.SaveContext = MtkMsdcSaveContext;
    InitializationData.RestoreContext = MtkMsdcRestoreContext;
    InitializationData.PowerControlCallback = MtkMsdcPowerControl;
    InitializationData.Cleanup = MtkMsdcCleanup;
    InitializationData.PrivateExtensionSize = sizeof(MTK_MSDC_EXTENSION);
    InitializationData.CrashdumpSupported = FALSE;

    return SdPortInitialize(DriverObject, RegistryPath, &InitializationData);
}

_Use_decl_annotations_
NTSTATUS
MtkMsdcGetSlotCount(
    PSD_MINIPORT Miniport,
    PUCHAR SlotCount
    )
{
    if (Miniport == NULL || SlotCount == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (Miniport->ConfigurationInfo.BusType != SdBusTypeAcpi) {
        return STATUS_NOT_SUPPORTED;
    }

    *SlotCount = 1;
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID
MtkMsdcGetSlotCapabilities(
    PVOID PrivateExtension,
    PSDPORT_CAPABILITIES Capabilities
    )
{
    PMTK_MSDC_EXTENSION Extension;

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    RtlCopyMemory(Capabilities,
                  &Extension->Capabilities,
                  sizeof(*Capabilities));
}

_Use_decl_annotations_
NTSTATUS
MtkMsdcInitialize(
    PVOID PrivateExtension,
    PHYSICAL_ADDRESS PhysicalBase,
    PVOID VirtualBase,
    ULONG Length,
    BOOLEAN CrashdumpMode
    )
{
    PMTK_MSDC_EXTENSION Extension;
    NTSTATUS Status;

    if (PrivateExtension == NULL || VirtualBase == NULL ||
        Length < MTK_MSDC_REQUIRED_MMIO_LENGTH) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    RtlZeroMemory(Extension, sizeof(*Extension));
    Extension->PhysicalBase = PhysicalBase;
    Extension->BaseAddress = VirtualBase;
    Extension->BaseAddressLength = Length;
    Extension->CrashdumpMode = CrashdumpMode;

    /*
     * This source rate is the LK/UEFI-selected MT6768 MSDC1 parent observed by
     * the working firmware path.  Production power/clock ownership will move
     * to a separate MT6768 clock driver after this read-only proof.
     */
    Extension->CurrentClockHz = 20000000;

    RtlZeroMemory(&Extension->Capabilities,
                  sizeof(Extension->Capabilities));
    Extension->Capabilities.SpecVersion = 3;
    Extension->Capabilities.MaximumOutstandingRequests = 1;
    Extension->Capabilities.MaximumBlockSize = 512;
    Extension->Capabilities.MaximumBlockCount = 1;
    Extension->Capabilities.BaseClockFrequencyKhz =
        MTK_MSDC_SOURCE_CLOCK_HZ / 1000;
    Extension->Capabilities.PioTransferMaxThreshold = MAXULONG;
    Extension->Capabilities.Supported.HighSpeed = 0;
    Extension->Capabilities.Supported.DriverTypeB = 1;
    Extension->Capabilities.Supported.Voltage33V = 1;
    Extension->Capabilities.Supported.Limit200mA = 1;
    Extension->Capabilities.Flags.UsePioForRead = 1;
    Extension->Capabilities.Flags.UsePioForWrite = 1;

    /* Keep the inherited clock tree, rail, pinmux, and MSDC-TOP state. */
    MtkMsdcSetBits(Extension,
                   MSDC_CFG,
                   MSDC_CFG_MODE | MSDC_CFG_CKPD | MSDC_CFG_PIO);
    Status = MtkMsdcRecover(Extension);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    MtkMsdcWrite(Extension, MSDC_INTEN, 0);
    MtkMsdcSetBits(Extension, SDC_CFG, SDC_CFG_SDIO);
    MtkMsdcClearBits(Extension, SDC_CFG, SDC_CFG_SDIOIDE);
    (VOID)MtkMsdcSetBusWidth(Extension, SdBusWidth1Bit);

    /* Proven MT6768 host quirks from Mu-Silicium MsdcDxe. */
    MtkMsdcWrite(Extension, MSDC_IOCON, 0);
    MtkMsdcWrite(Extension, MSDC_PATCH_BIT0, 0x403c0446);
    MtkMsdcWrite(Extension, MSDC_PATCH_BIT1, 0xffff4089);
    MtkMsdcClearBits(Extension, MSDC_PATCH_BIT1, MSDC_PATCH1_BUSY_CHECK);
    MtkMsdcSetBits(Extension, MSDC_PATCH_BIT1, MTK_BIT(8) | MTK_BIT(9));
    MtkMsdcClearBits(Extension,
                     SDC_FIFO_CFG,
                     SDC_FIFO_CFG_WRVALIDSEL | SDC_FIFO_CFG_RDVALIDSEL);
    MtkMsdcSetBits(Extension, EMMC50_CFG0, EMMC50_CFG0_CRCSTSSEL);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MtkMsdcIssueBusOperation(
    PVOID PrivateExtension,
    PSDPORT_BUS_OPERATION BusOperation
    )
{
    PMTK_MSDC_EXTENSION Extension;

    if (PrivateExtension == NULL || BusOperation == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;

    switch (BusOperation->Type) {
    case SdResetHw:
    case SdResetHost:
        return MtkMsdcRecover(Extension);

    case SdSetClock:
        if (BusOperation->Parameters.FrequencyKhz > MAXULONG / 1000) {
            return STATUS_INVALID_PARAMETER;
        }
        return MtkMsdcSetClock(
            Extension,
            BusOperation->Parameters.FrequencyKhz * 1000);

    case SdSetVoltage:
        if (BusOperation->Parameters.Voltage == SdBusVoltage33 ||
            BusOperation->Parameters.Voltage == SdBusVoltageOff) {
            return STATUS_SUCCESS;
        }
        return STATUS_NOT_SUPPORTED;

    case SdSetPower:
        /* The first test preserves LK/UEFI's already-enabled VMCH/VMC rails. */
        return STATUS_SUCCESS;

    case SdSetBusWidth:
        return MtkMsdcSetBusWidth(Extension,
                                  BusOperation->Parameters.BusWidth);

    case SdSetBusSpeed:
        if (BusOperation->Parameters.BusSpeed == SdBusSpeedNormal ||
            BusOperation->Parameters.BusSpeed == SdBusSpeedUndefined) {
            return STATUS_SUCCESS;
        }
        return STATUS_NOT_SUPPORTED;

    case SdSetSignalingVoltage:
        return BusOperation->Parameters.SignalingVoltage ==
                   SdSignalingVoltage33
               ? STATUS_SUCCESS
               : STATUS_NOT_SUPPORTED;

    case SdSetDriverType:
        return BusOperation->Parameters.DriverType == SdDriverTypeB
               ? STATUS_SUCCESS
               : STATUS_NOT_SUPPORTED;

    case SdSetPresetValue:
        return BusOperation->Parameters.PresetValueEnabled == FALSE
               ? STATUS_SUCCESS
               : STATUS_NOT_SUPPORTED;

    case SdSetBlockGapInterrupt:
        return BusOperation->Parameters.BlockGapIntEnabled == FALSE
               ? STATUS_SUCCESS
               : STATUS_NOT_SUPPORTED;

    case SdSetDriveStrength:
    case SdExecuteTuning:
    default:
        return STATUS_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
BOOLEAN
MtkMsdcGetCardDetectState(
    PVOID PrivateExtension
    )
{
    UNREFERENCED_PARAMETER(PrivateExtension);

    /*
     * Host1 uses external GPIO18, not MSDC_PS.CDSTS.  Until GpioClx support is
     * present, this diagnostic package is intentionally for an inserted card.
     */
    return TRUE;
}

_Use_decl_annotations_
BOOLEAN
MtkMsdcGetWriteProtectState(
    PVOID PrivateExtension
    )
{
    UNREFERENCED_PARAMETER(PrivateExtension);
    return TRUE;
}

_Use_decl_annotations_
BOOLEAN
MtkMsdcInterrupt(
    PVOID PrivateExtension,
    PULONG Events,
    PULONG Errors,
    PBOOLEAN NotifyCardChange,
    PBOOLEAN NotifySdioInterrupt,
    PBOOLEAN NotifyTuning
    )
{
    PMTK_MSDC_EXTENSION Extension;
    ULONG Pending;

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    *Events = 0;
    *Errors = 0;
    *NotifyCardChange = FALSE;
    *NotifySdioInterrupt = FALSE;
    *NotifyTuning = FALSE;

    Pending = MtkMsdcRead(Extension, MSDC_INT) &
              MtkMsdcRead(Extension, MSDC_INTEN);
    if (Pending == 0) {
        return FALSE;
    }

    MtkMsdcWrite(Extension, MSDC_INT, Pending);
    if ((Pending & MSDC_INT_CMDRDY) != 0) {
        *Events |= SDPORT_EVENT_CARD_RESPONSE;
    }
    if ((Pending & MSDC_INT_XFER_COMPL) != 0) {
        *Events |= SDPORT_EVENT_CARD_RW_END;
    }
    if ((Pending & MSDC_INT_CDSC) != 0) {
        *NotifyCardChange = TRUE;
    }
    if ((Pending & MSDC_INT_MMCIRQ) != 0) {
        *NotifySdioInterrupt = TRUE;
    }

    *Errors = MtkMsdcErrorsFromInterrupt(Pending);
    if (*Errors != 0) {
        *Events |= SDPORT_EVENT_ERROR;
    }

    return TRUE;
}

_Use_decl_annotations_
NTSTATUS
MtkMsdcIssueRequest(
    PVOID PrivateExtension,
    PSDPORT_REQUEST Request
    )
{
    PMTK_MSDC_EXTENSION Extension;

    if (PrivateExtension == NULL || Request == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;

    switch (Request->Type) {
    case SdRequestTypeCommandNoTransfer:
    case SdRequestTypeCommandWithTransfer:
        return MtkMsdcIssueCommand(Extension, Request);
    case SdRequestTypeStartTransfer:
        return MtkMsdcStartTransfer(Extension, Request);
    default:
        return STATUS_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
VOID
MtkMsdcGetResponse(
    PVOID PrivateExtension,
    PSDPORT_COMMAND Command,
    PVOID ResponseBuffer
    )
{
    PMTK_MSDC_EXTENSION Extension;
    UCHAR RawResponse[SDPORT_MAX_RESPONSE_LENGTH];

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    RtlZeroMemory(ResponseBuffer, SDPORT_MAX_RESPONSE_LENGTH);

    if (Command->ResponseType == SdResponseTypeNone) {
        return;
    }
    if (Command->ResponseType != SdResponseTypeR2) {
        RtlCopyMemory(ResponseBuffer,
                      &Extension->Response[0],
                      sizeof(Extension->Response[0]));
        return;
    }

    /*
     * MTK exposes the full 128 response bits.  SDPORT consumes the standard
     * SDHC 136-bit response window, which omits the low CRC byte.  Apply the
     * same one-byte compaction used by the proven Mu-Silicium pass-thru path.
     */
    RtlCopyMemory(RawResponse,
                  Extension->Response,
                  sizeof(RawResponse));
    RtlCopyMemory(ResponseBuffer,
                  RawResponse + 1,
                  SDPORT_MAX_RESPONSE_LENGTH - 1);
}

_Use_decl_annotations_
VOID
MtkMsdcRequestDpc(
    PVOID PrivateExtension,
    PSDPORT_REQUEST Request,
    ULONG Events,
    ULONG Errors
    )
{
    UNREFERENCED_PARAMETER(PrivateExtension);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Events);
    UNREFERENCED_PARAMETER(Errors);

    /* All request completions in the first diagnostic build are synchronous. */
}

static ULONG
MtkMsdcInterruptMaskFromEvents(
    _In_ ULONG EventMask
    )
{
    ULONG InterruptMask;

    InterruptMask = 0;
    if ((EventMask & SDPORT_EVENT_CARD_CHANGE) != 0) {
        InterruptMask |= MSDC_INT_CDSC;
    }
    if ((EventMask & SDPORT_EVENT_CARD_INTERRUPT) != 0) {
        InterruptMask |= MSDC_INT_MMCIRQ;
    }

    /* Request-completion events are polled in this first safe build. */
    return InterruptMask;
}

_Use_decl_annotations_
VOID
MtkMsdcToggleEvents(
    PVOID PrivateExtension,
    ULONG EventMask,
    BOOLEAN Enable
    )
{
    PMTK_MSDC_EXTENSION Extension;
    ULONG InterruptMask;

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    InterruptMask = MtkMsdcInterruptMaskFromEvents(EventMask);
    if (Enable != FALSE) {
        MtkMsdcSetBits(Extension, MSDC_INTEN, InterruptMask);
    } else {
        MtkMsdcClearBits(Extension, MSDC_INTEN, InterruptMask);
    }
}

_Use_decl_annotations_
VOID
MtkMsdcClearEvents(
    PVOID PrivateExtension,
    ULONG EventMask
    )
{
    PMTK_MSDC_EXTENSION Extension;
    ULONG InterruptMask;

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    InterruptMask = MtkMsdcInterruptMaskFromEvents(EventMask);
    if (InterruptMask != 0) {
        MtkMsdcWrite(Extension, MSDC_INT, InterruptMask);
    }
}

_Use_decl_annotations_
VOID
MtkMsdcSaveContext(
    PVOID PrivateExtension
    )
{
    UNREFERENCED_PARAMETER(PrivateExtension);
}

_Use_decl_annotations_
VOID
MtkMsdcRestoreContext(
    PVOID PrivateExtension
    )
{
    UNREFERENCED_PARAMETER(PrivateExtension);
}

_Use_decl_annotations_
NTSTATUS
MtkMsdcPowerControl(
    PSD_MINIPORT Miniport,
    LPCGUID PowerControlCode,
    PVOID InputBuffer,
    SIZE_T InputBufferSize,
    PVOID OutputBuffer,
    SIZE_T OutputBufferSize,
    PSIZE_T BytesReturned
    )
{
    UNREFERENCED_PARAMETER(Miniport);
    UNREFERENCED_PARAMETER(PowerControlCode);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);
    return STATUS_NOT_SUPPORTED;
}

_Use_decl_annotations_
VOID
MtkMsdcCleanup(
    PSD_MINIPORT Miniport
    )
{
    UNREFERENCED_PARAMETER(Miniport);
}

