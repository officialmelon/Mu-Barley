#include "mtkmsdc.h"

#define MTK_MSDC_CONTROL_TIMEOUT_US  100000UL
#define MTK_MSDC_COMMAND_TIMEOUT_US 1000000UL
#define MTK_MSDC_DATA_TIMEOUT_US    5000000UL
#define MTK_MSDC_BUSY_TIMEOUT_US      20000UL
#define MTK_MSDC_REGISTRY_PATH_CHARS    512UL

#define MTK_MSDC_LOG(_level, _format, ...)                              \
    KdPrintEx((DPFLTR_IHVDRIVER_ID, (_level),                           \
               "mtkmsdc: " _format, __VA_ARGS__))

typedef struct _MTK_MSDC_DIAG_VALUE {
    PCWSTR Name;
    ULONG Value;
} MTK_MSDC_DIAG_VALUE, *PMTK_MSDC_DIAG_VALUE;

static WCHAR gMtkMsdcRegistryPathBuffer[MTK_MSDC_REGISTRY_PATH_CHARS];
static UNICODE_STRING gMtkMsdcRegistryPath;

static VOID
MtkMsdcDiagWorker(
    _In_ PVOID Parameter
    )
{
    PMTK_MSDC_EXTENSION Extension;
    OBJECT_ATTRIBUTES Attributes;
    MTK_MSDC_DIAG_VALUE Values[] = {
        { L"IssueCount", 0 },
        { L"IsrCount", 0 },
        { L"DpcCount", 0 },
        { L"CompleteCount", 0 },
        { L"CommandPhaseCount", 0 },
        { L"StartTransferCount", 0 },
        { L"BusyRejectCount", 0 },
        { L"StaleDpcCount", 0 },
        { L"CardDetectCount", 0 },
        { L"LastRequestType", 0 },
        { L"LastCommand", 0 },
        { L"LastArgument", 0 },
        { L"LastResponseType", 0 },
        { L"LastTransferType", 0 },
        { L"LastDirection", 0 },
        { L"LastBlockSize", 0 },
        { L"LastBlockCount", 0 },
        { L"LastLength", 0 },
        { L"LastRequiredEvents", 0 },
        { L"LastEvents", 0 },
        { L"LastErrors", 0 },
        { L"LastRawInterrupt", 0 },
        { L"LastIntEnable", 0 },
        { L"LastSdcStatus", 0 },
        { L"LastFifoStatus", 0 },
        { L"Resp0", 0 },
        { L"Resp1", 0 },
        { L"Resp2", 0 },
        { L"Resp3", 0 },
        { L"LastCompletionStatus", 0 },
        { L"LastTimeoutStage", 0 },
        { L"LastCardPresent", 0 },
        { L"CurrentClockHz", 0 },
        { L"CurrentBusWidth", 0 },
        { L"Cid0", 0 },
        { L"Cid1", 0 },
        { L"Cid2", 0 },
        { L"Cid3", 0 },
        { L"Csd0", 0 },
        { L"Csd1", 0 },
        { L"Csd2", 0 },
        { L"Csd3", 0 },
        { L"CsdLate0", 0 },
        { L"CsdLate1", 0 },
        { L"CsdLate2", 0 },
        { L"CsdLate3", 0 },
        { L"CidCrcOk", 0 },
        { L"CsdCrcOk", 0 },
        { L"CsdStruct", 0 },
        { L"CsdCapKb", 0 },
        { L"Snap0R0", 0 }, { L"Snap0R1", 0 }, { L"Snap0R2", 0 }, { L"Snap0R3", 0 },
        { L"Snap1R0", 0 }, { L"Snap1R1", 0 }, { L"Snap1R2", 0 }, { L"Snap1R3", 0 },
        { L"Snap2R0", 0 }, { L"Snap2R1", 0 }, { L"Snap2R2", 0 }, { L"Snap2R3", 0 },
        { L"Snap3R0", 0 }, { L"Snap3R1", 0 }, { L"Snap3R2", 0 }, { L"Snap3R3", 0 },
        { L"Gr2Idx", 0 }, { L"Gr2R0", 0 }, { L"Gr2R1", 0 }, { L"Gr2R2", 0 }, { L"Gr2R3", 0 },
        { L"OcrValue", 0 },
        { L"CsdRepaired", 0 },
        { L"TraceSequence", 0 }
    };
    HANDLE Key;
    ULONG CharacterIndex;
    ULONG Index;
    WCHAR NameBuffer[64];
    UNICODE_STRING Name;

    Extension = (PMTK_MSDC_EXTENSION)Parameter;
    if (Extension == NULL || gMtkMsdcRegistryPath.Buffer == NULL) {
        if (Extension != NULL) {
            InterlockedExchange(&Extension->DiagWorkQueued, 0);
        }
        return;
    }

    Values[0].Value = (ULONG)Extension->DiagIssueCount;
    Values[1].Value = (ULONG)Extension->DiagIsrCount;
    Values[2].Value = (ULONG)Extension->DiagDpcCount;
    Values[3].Value = (ULONG)Extension->DiagCompleteCount;
    Values[4].Value = (ULONG)Extension->DiagCommandPhaseCount;
    Values[5].Value = (ULONG)Extension->DiagStartTransferCount;
    Values[6].Value = (ULONG)Extension->DiagBusyRejectCount;
    Values[7].Value = (ULONG)Extension->DiagStaleDpcCount;
    Values[8].Value = (ULONG)Extension->DiagCardDetectCount;
    Values[9].Value = Extension->DiagLastRequestType;
    Values[10].Value = Extension->DiagLastCommand;
    Values[11].Value = Extension->DiagLastArgument;
    Values[12].Value = Extension->DiagLastResponseType;
    Values[13].Value = Extension->DiagLastTransferType;
    Values[14].Value = Extension->DiagLastDirection;
    Values[15].Value = Extension->DiagLastBlockSize;
    Values[16].Value = Extension->DiagLastBlockCount;
    Values[17].Value = Extension->DiagLastLength;
    Values[18].Value = Extension->DiagLastRequiredEvents;
    Values[19].Value = Extension->DiagLastEvents;
    Values[20].Value = Extension->DiagLastErrors;
    Values[21].Value = Extension->DiagLastRawInterrupt;
    Values[22].Value = Extension->DiagLastIntEnable;
    Values[23].Value = Extension->DiagLastSdcStatus;
    Values[24].Value = Extension->DiagLastFifoStatus;
    Values[25].Value = Extension->Response[0];
    Values[26].Value = Extension->Response[1];
    Values[27].Value = Extension->Response[2];
    Values[28].Value = Extension->Response[3];
    Values[29].Value = Extension->DiagLastCompletionStatus;
    Values[30].Value = Extension->DiagLastTimeoutStage;
    Values[31].Value = Extension->DiagLastCardPresent;
    Values[32].Value = Extension->CurrentClockHz;
    Values[33].Value = Extension->DiagCurrentBusWidth;
    Values[34].Value = Extension->DiagCidResponse[0];
    Values[35].Value = Extension->DiagCidResponse[1];
    Values[36].Value = Extension->DiagCidResponse[2];
    Values[37].Value = Extension->DiagCidResponse[3];
    Values[38].Value = Extension->DiagCsdResponse[0];
    Values[39].Value = Extension->DiagCsdResponse[1];
    Values[40].Value = Extension->DiagCsdResponse[2];
    Values[41].Value = Extension->DiagCsdResponse[3];
    Values[42].Value = Extension->DiagCsdLateResponse[0];
    Values[43].Value = Extension->DiagCsdLateResponse[1];
    Values[44].Value = Extension->DiagCsdLateResponse[2];
    Values[45].Value = Extension->DiagCsdLateResponse[3];
    Values[46].Value = Extension->DiagCidCrcOk;
    Values[47].Value = Extension->DiagCsdCrcOk;
    Values[48].Value = Extension->DiagCsdStructure;
    Values[49].Value = Extension->DiagCsdCapKb;
    Values[50].Value = Extension->DiagSnap[0][0];
    Values[51].Value = Extension->DiagSnap[0][1];
    Values[52].Value = Extension->DiagSnap[0][2];
    Values[53].Value = Extension->DiagSnap[0][3];
    Values[54].Value = Extension->DiagSnap[1][0];
    Values[55].Value = Extension->DiagSnap[1][1];
    Values[56].Value = Extension->DiagSnap[1][2];
    Values[57].Value = Extension->DiagSnap[1][3];
    Values[58].Value = Extension->DiagSnap[2][0];
    Values[59].Value = Extension->DiagSnap[2][1];
    Values[60].Value = Extension->DiagSnap[2][2];
    Values[61].Value = Extension->DiagSnap[2][3];
    Values[62].Value = Extension->DiagSnap[3][0];
    Values[63].Value = Extension->DiagSnap[3][1];
    Values[64].Value = Extension->DiagSnap[3][2];
    Values[65].Value = Extension->DiagSnap[3][3];
    Values[66].Value = Extension->DiagGr2Index;
    Values[67].Value = Extension->DiagGr2Resp[0];
    Values[68].Value = Extension->DiagGr2Resp[1];
    Values[69].Value = Extension->DiagGr2Resp[2];
    Values[70].Value = Extension->DiagGr2Resp[3];
    Values[71].Value = Extension->DiagOcrValue;
    Values[72].Value = Extension->DiagCsdRepaired;
    Values[73].Value = (ULONG)Extension->DiagTraceSequence;

    InitializeObjectAttributes(&Attributes,
                               &gMtkMsdcRegistryPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);
    if (NT_SUCCESS(ZwOpenKey(&Key, KEY_SET_VALUE, &Attributes))) {
        for (Index = 0; Index < RTL_NUMBER_OF(Values); Index += 1) {
            NameBuffer[0] = L'D';
            NameBuffer[1] = L'i';
            NameBuffer[2] = L'a';
            NameBuffer[3] = L'g';
            NameBuffer[4] = L'H';
            NameBuffer[5] = (WCHAR)(L'0' + Extension->HostIndex);
            for (CharacterIndex = 0;
                 CharacterIndex + 7 < RTL_NUMBER_OF(NameBuffer) &&
                 Values[Index].Name[CharacterIndex] != L'\0';
                 CharacterIndex += 1) {
                NameBuffer[6 + CharacterIndex] =
                    Values[Index].Name[CharacterIndex];
            }
            NameBuffer[6 + CharacterIndex] = L'\0';
            if (Values[Index].Name[CharacterIndex] == L'\0') {
                RtlInitUnicodeString(&Name, NameBuffer);
                (VOID)ZwSetValueKey(Key,
                                    &Name,
                                    0,
                                    REG_DWORD,
                                    &Values[Index].Value,
                                    sizeof(Values[Index].Value));
            }
        }

        for (Index = 0; Index < MTK_MSDC_TRACE_DEPTH; Index += 1) {
            ULONG TraceValue;

            NameBuffer[0] = L'D';
            NameBuffer[1] = L'i';
            NameBuffer[2] = L'a';
            NameBuffer[3] = L'g';
            NameBuffer[4] = L'H';
            NameBuffer[5] = (WCHAR)(L'0' + Extension->HostIndex);
            NameBuffer[6] = L'T';
            NameBuffer[7] = L'r';
            NameBuffer[8] = L'a';
            NameBuffer[9] = L'c';
            NameBuffer[10] = L'e';
            NameBuffer[11] = (WCHAR)(L'0' + (Index / 10));
            NameBuffer[12] = (WCHAR)(L'0' + (Index % 10));
            NameBuffer[13] = L'R';
            NameBuffer[14] = L'e';
            NameBuffer[15] = L'q';
            NameBuffer[16] = L'\0';
            RtlInitUnicodeString(&Name, NameBuffer);
            TraceValue = Extension->DiagTraceRequest[Index];
            (VOID)ZwSetValueKey(Key,
                                &Name,
                                0,
                                REG_DWORD,
                                &TraceValue,
                                sizeof(TraceValue));

            NameBuffer[13] = L'A';
            NameBuffer[14] = L'r';
            NameBuffer[15] = L'g';
            RtlInitUnicodeString(&Name, NameBuffer);
            TraceValue = Extension->DiagTraceArgument[Index];
            (VOID)ZwSetValueKey(Key,
                                &Name,
                                0,
                                REG_DWORD,
                                &TraceValue,
                                sizeof(TraceValue));
        }
        ZwClose(Key);
    }

    InterlockedExchange(&Extension->DiagWorkQueued, 0);
}

static VOID
MtkMsdcQueueDiagWork(
    _In_ PMTK_MSDC_EXTENSION Extension
    )
{
    if (Extension->CrashdumpMode == FALSE &&
        gMtkMsdcRegistryPath.Buffer != NULL &&
        InterlockedCompareExchange(&Extension->DiagWorkQueued, 1, 0) == 0) {
        ExQueueWorkItem(&Extension->DiagWorkItem, DelayedWorkQueue);
    }
}

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
    case SdBusWidth8Bit:
        if (Extension->IsEmmc == FALSE) {
            return STATUS_NOT_SUPPORTED;
        }
        EncodedWidth = MSDC_BUS_WIDTH_8;
        break;
    default:
        return STATUS_NOT_SUPPORTED;
    }

    Value = MtkMsdcRead(Extension, SDC_CFG);
    Value &= ~SDC_CFG_BUS_WIDTH_MASK;
    Value |= EncodedWidth << SDC_CFG_BUS_WIDTH_SHIFT;
    MtkMsdcWrite(Extension, SDC_CFG, Value);
    Extension->DiagCurrentBusWidth = (ULONG)Width;
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
MtkMsdcPollCommandCompletion(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _Out_ PULONG InterruptStatus
    )
{
    ULONG Pending;
    ULONG Poll;

    *InterruptStatus = 0;
    for (Poll = 0; Poll < MTK_MSDC_COMMAND_TIMEOUT_US; Poll += 1) {
        Pending = MtkMsdcRead(Extension, MSDC_INT) & MSDC_INT_CMD_STATUS;
        if (Pending != 0) {
            *InterruptStatus = Pending;
            MtkMsdcWrite(Extension, MSDC_INT, Pending);

            if ((Pending & MSDC_INT_CMD_ERROR) != 0) {
                return MtkMsdcStatusFromInterrupt(Pending);
            }

            if ((Pending & MSDC_INT_CMDRDY) != 0) {
                MtkMsdcCaptureResponse(Extension);
                return STATUS_SUCCESS;
            }
        }

        SdPortWait(1);
    }

    return STATUS_IO_TIMEOUT;
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
    ULONG InterruptStatus;
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
        if (Command->TransferDirection == SdTransferDirectionWrite) {
            RawCommand |= SDC_CMD_RW;
        }
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

    /*
     * Command requests are owned by SDPORT once this callback returns
     * STATUS_PENDING.  Completing a command from inside IssueRequest races
     * the port driver's outstanding-request bookkeeping and caused the
     * observed bugcheck 0x139.  Arm the real controller interrupt and let
     * SDPORT invoke MtkMsdcRequestDpc after IssueRequest has returned.
     *
     * Data movement remains bounded PIO in the later StartTransfer phase, so
     * the command phase waits only for the card-response event here.
     */
    MtkMsdcClearBits(Extension, MSDC_INTEN, MSDC_INT_CMD_STATUS);
    MtkMsdcClearAllInterrupts(Extension);
    Request->RequiredEvents = SDPORT_EVENT_CARD_RESPONSE;
    if (HasData) {
        /*
         * SDPORT advances a PIO command into SdRequestTypeStartTransfer only
         * after the miniport reports the appropriate buffer-ready event.
         * MTK exposes FIFO fill counts instead of a dedicated threshold IRQ,
         * so the ISR synthesizes this event with command completion and the
         * transfer routine then polls the real FIFO count safely.
         */
        Request->RequiredEvents |=
            Command->TransferDirection == SdTransferDirectionRead
                ? SDPORT_EVENT_BUFFER_FULL
                : SDPORT_EVENT_BUFFER_EMPTY;
    }
    MtkMsdcWrite(Extension, SDC_ARG, Command->Argument);

    /*
     * MSDC0 has repeatedly failed to deliver the interrupt edge for eMMC
     * CMD3 even though every preceding command completes normally.  Do this
     * one short command synchronously, before SDPORT records it as pending.
     * This both preserves the response and guarantees that a missing edge can
     * delay enumeration for at most the miniport command timeout instead of
     * wedging sdbus for roughly 30 seconds.
     */
    if (Extension->IsEmmc != FALSE &&
        Command->Index == 3 &&
        HasData == FALSE) {
        MtkMsdcWrite(Extension, SDC_CMD, RawCommand);
        Status = MtkMsdcPollCommandCompletion(Extension, &InterruptStatus);
        Extension->DiagLastRawInterrupt = InterruptStatus;
        Extension->DiagLastIntEnable = MtkMsdcRead(Extension, MSDC_INTEN);
        Extension->DiagLastSdcStatus = MtkMsdcRead(Extension, SDC_STS);
        Extension->DiagLastFifoStatus = MtkMsdcRead(Extension, MSDC_FIFOCS);
        Request->RequiredEvents = 0;
        Request->Status = Status;

        if (!NT_SUCCESS(Status)) {
            (VOID)MtkMsdcRecover(Extension);
        }
        return Status;
    }

    MtkMsdcSetBits(Extension, MSDC_INTEN, MSDC_INT_CMD_STATUS);
    MtkMsdcWrite(Extension, SDC_CMD, RawCommand);
    return STATUS_PENDING;
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

        if ((Observed & ~MSDC_INT_XFER_COMPL) != 0) {
            MtkMsdcWrite(Extension,
                         MSDC_INT,
                         Observed & ~MSDC_INT_XFER_COMPL);
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

static VOID
MtkMsdcWriteFifo(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_reads_bytes_(Length) const UCHAR *Buffer,
    _In_ ULONG Length
    )
{
    ULONG Value;

    while (Length >= sizeof(Value)) {
        RtlCopyMemory(&Value, Buffer, sizeof(Value));
        SdPortWriteRegisterUlong(Extension->BaseAddress, MSDC_TXDATA, Value);
        Buffer += sizeof(Value);
        Length -= sizeof(Value);
    }

    while (Length != 0) {
        SdPortWriteRegisterUchar(Extension->BaseAddress, MSDC_TXDATA, *Buffer);
        Buffer += 1;
        Length -= 1;
    }
}

static NTSTATUS
MtkMsdcPioWrite(
    _In_ PMTK_MSDC_EXTENSION Extension,
    _In_reads_bytes_(Length) const UCHAR *Buffer,
    _In_ ULONG Length
    )
{
    BOOLEAN TransferComplete;
    ULONG Available;
    ULONG Chunk;
    ULONG InterruptStatus;
    ULONG Observed;
    ULONG Poll;
    ULONG Remaining;
    ULONG TxCount;

    if (Buffer == NULL || Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    Remaining = Length;
    TransferComplete = FALSE;
    InterruptStatus = 0;

    for (Poll = 0; Poll < MTK_MSDC_DATA_TIMEOUT_US; Poll += 1) {
        InterruptStatus = MtkMsdcRead(Extension, MSDC_INT);
        Observed = InterruptStatus & MSDC_INT_DATA_STATUS;

        TxCount = (MtkMsdcRead(Extension, MSDC_FIFOCS) &
                   MSDC_FIFOCS_TXCNT_MASK) >> MSDC_FIFOCS_TXCNT_SHIFT;
        Available = (TxCount < MSDC_FIFO_SIZE) ? MSDC_FIFO_SIZE - TxCount : 0;
        Chunk = (Remaining < Available) ? Remaining : Available;
        if (Chunk != 0) {
            MtkMsdcWriteFifo(Extension, Buffer, Chunk);
            Buffer += Chunk;
            Remaining -= Chunk;
        }

        if ((Observed & ~MSDC_INT_XFER_COMPL) != 0) {
            MtkMsdcWrite(Extension,
                         MSDC_INT,
                         Observed & ~MSDC_INT_XFER_COMPL);
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
                 "PIO write timed out, remaining=%lu int=%08lx\n",
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
    if (Request->Command.TransferDirection != SdTransferDirectionRead &&
        Request->Command.TransferDirection != SdTransferDirectionWrite) {
        return STATUS_INVALID_PARAMETER;
    }

    Length = Request->Command.Length;
    if (Length == 0 && Request->Command.BlockSize != 0 &&
        Request->Command.BlockCount != 0 &&
        Request->Command.BlockCount <= MAXULONG / Request->Command.BlockSize) {
        Length = Request->Command.BlockSize * Request->Command.BlockCount;
    }
    if (Length == 0 || Request->Command.DataBuffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Request->Command.TransferDirection == SdTransferDirectionRead) {
        Status = MtkMsdcPioRead(Extension,
                                Request->Command.DataBuffer,
                                Length);
    } else {
        Status = MtkMsdcPioWrite(Extension,
                                 Request->Command.DataBuffer,
                                 Length);
    }
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

    if (RegistryPath == NULL || RegistryPath->Buffer == NULL ||
        RegistryPath->Length + sizeof(WCHAR) >
            sizeof(gMtkMsdcRegistryPathBuffer)) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(gMtkMsdcRegistryPathBuffer,
                  sizeof(gMtkMsdcRegistryPathBuffer));
    RtlCopyMemory(gMtkMsdcRegistryPathBuffer,
                  RegistryPath->Buffer,
                  RegistryPath->Length);
    gMtkMsdcRegistryPath.Buffer = gMtkMsdcRegistryPathBuffer;
    gMtkMsdcRegistryPath.Length = RegistryPath->Length;
    gMtkMsdcRegistryPath.MaximumLength =
        (USHORT)sizeof(gMtkMsdcRegistryPathBuffer);

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
    if ((ULONGLONG)PhysicalBase.QuadPart == MTK_MSDC0_BASE) {
        Extension->HostIndex = 0;
        Extension->IsEmmc = TRUE;
    } else if ((ULONGLONG)PhysicalBase.QuadPart == MTK_MSDC1_BASE) {
        Extension->HostIndex = 1;
        Extension->IsEmmc = FALSE;
    } else {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    /*
     * Both controllers inherit the 320 MHz parent selected by the proven
     * LK/UEFI path. A future clock-controller driver can own that mux; this
     * miniport only programs MSDC_CFG's local divider.
     */
    Extension->CurrentClockHz = 20000000;
    Extension->OutstandingRequest = NULL;
    ExInitializeWorkItem(&Extension->DiagWorkItem,
                         MtkMsdcDiagWorker,
                         Extension);

    RtlZeroMemory(&Extension->Capabilities,
                  sizeof(Extension->Capabilities));
    Extension->Capabilities.SpecVersion = 3;
    Extension->Capabilities.MaximumOutstandingRequests = 1;
    Extension->Capabilities.MaximumBlockSize = 512;
    Extension->Capabilities.MaximumBlockCount = MAXUSHORT;
    Extension->Capabilities.BaseClockFrequencyKhz =
        MTK_MSDC_SOURCE_CLOCK_HZ / 1000;
    Extension->Capabilities.PioTransferMaxThreshold = MAXULONG;
    Extension->Capabilities.Supported.HighSpeed = 0;
    Extension->Capabilities.Supported.BusWidth8Bit = Extension->IsEmmc;
    Extension->Capabilities.Supported.DriverTypeB = 1;
    Extension->Capabilities.Supported.Voltage33V = 1;
    Extension->Capabilities.Supported.Limit200mA = 1;
    /*
     * Keep the first proven PIO path single-purpose.  MTK auto-CMD12 has its
     * own completion/error bits; advertising it before that second command
     * path is implemented can hide an otherwise successful data transfer.
     */
    Extension->Capabilities.Supported.AutoCmd12 = 0;
    Extension->Capabilities.Flags.UsePioForRead = 1;
    Extension->Capabilities.Flags.UsePioForWrite = 1;

    /* Keep the inherited clock tree, rails, pinmux, and MSDC-TOP state. */
    MtkMsdcSetBits(Extension,
                   MSDC_CFG,
                   MSDC_CFG_MODE | MSDC_CFG_CKPD | MSDC_CFG_PIO);
    Status = MtkMsdcRecover(Extension);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    MtkMsdcWrite(Extension, MSDC_INTEN, 0);
    /* Memory-only hosts do not need SDIO mode (which is only used for CMD5). */
    MtkMsdcClearBits(Extension, SDC_CFG, SDC_CFG_SDIO | SDC_CFG_SDIOIDE);
    (VOID)MtkMsdcSetBusWidth(Extension, SdBusWidth1Bit);
    Status = MtkMsdcSetClock(Extension, 400000);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

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
        MtkMsdcWrite(Extension, MSDC_INTEN, 0);
        (VOID)InterlockedExchangePointer(
            &Extension->OutstandingRequest,
            NULL);
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
    PMTK_MSDC_EXTENSION Extension;

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    if (Extension == NULL) {
        return FALSE;
    }

    InterlockedIncrement(&Extension->DiagCardDetectCount);
    Extension->DiagLastCardPresent = TRUE;
    MtkMsdcQueueDiagWork(Extension);

    /*
     * MSDC0 is soldered eMMC. MSDC1 uses external GPIO18 rather than
     * MSDC_PS.CDSTS; until GpioClx support lands, the removable controller is
     * intentionally used with a card present at boot.
     */
    return TRUE;
}

_Use_decl_annotations_
BOOLEAN
MtkMsdcGetWriteProtectState(
    PVOID PrivateExtension
    )
{
    return PrivateExtension == NULL ? TRUE : FALSE;
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
    ULONG Enabled;
    ULONG Pending;
    ULONG Raw;

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    *Events = 0;
    *Errors = 0;
    *NotifyCardChange = FALSE;
    *NotifySdioInterrupt = FALSE;
    *NotifyTuning = FALSE;

    Raw = MtkMsdcRead(Extension, MSDC_INT);
    Enabled = MtkMsdcRead(Extension, MSDC_INTEN);
    Pending = Raw & Enabled;
    if (Pending == 0) {
        return FALSE;
    }

    InterlockedIncrement(&Extension->DiagIsrCount);
    Extension->DiagLastRawInterrupt = Raw;
    Extension->DiagLastIntEnable = Enabled;

    if ((Pending & MSDC_INT_CMDRDY) != 0) {
        PSDPORT_REQUEST Outstanding;

        *Events |= SDPORT_EVENT_CARD_RESPONSE;
        Outstanding = (PSDPORT_REQUEST)InterlockedCompareExchangePointer(
            &Extension->OutstandingRequest,
            NULL,
            NULL);
        if (Outstanding != NULL &&
            Outstanding->Type == SdRequestTypeCommandWithTransfer) {
            if (Outstanding->Command.TransferDirection ==
                SdTransferDirectionRead) {
                *Events |= SDPORT_EVENT_BUFFER_FULL;
            } else if (Outstanding->Command.TransferDirection ==
                       SdTransferDirectionWrite) {
                *Events |= SDPORT_EVENT_BUFFER_EMPTY;
            }
        }
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
    Extension->DiagLastEvents = *Events;
    Extension->DiagLastErrors = *Errors;
    Extension->DiagLastSdcStatus = MtkMsdcRead(Extension, SDC_STS);
    Extension->DiagLastFifoStatus = MtkMsdcRead(Extension, MSDC_FIFOCS);
    MtkMsdcWrite(Extension, MSDC_INT, Pending);
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
    NTSTATUS Status;

    if (PrivateExtension == NULL || Request == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;

    InterlockedIncrement(&Extension->DiagIssueCount);
    {
        LONG Sequence;
        ULONG TraceIndex;

        Sequence = InterlockedIncrement(&Extension->DiagTraceSequence) - 1;
        TraceIndex = (ULONG)Sequence % MTK_MSDC_TRACE_DEPTH;
        Extension->DiagTraceRequest[TraceIndex] =
            (((ULONG)Request->Type & 0x0f) << 28) |
            (((ULONG)Request->Command.Index & 0x3f) << 22) |
            (((ULONG)Request->Command.ResponseType & 0x0f) << 18) |
            (((ULONG)Request->Command.TransferType & 0x0f) << 14) |
            ((ULONG)Request->Command.BlockSize & 0x3fff);
        Extension->DiagTraceArgument[TraceIndex] = Request->Command.Argument;
    }
    Extension->DiagLastRequestType = (ULONG)Request->Type;
    Extension->DiagLastCommand = Request->Command.Index;
    Extension->DiagLastArgument = Request->Command.Argument;
    Extension->DiagLastResponseType = (ULONG)Request->Command.ResponseType;
    Extension->DiagLastTransferType = (ULONG)Request->Command.TransferType;
    Extension->DiagLastDirection =
        (ULONG)Request->Command.TransferDirection;
    Extension->DiagLastBlockSize = Request->Command.BlockSize;
    Extension->DiagLastBlockCount = Request->Command.BlockCount;
    Extension->DiagLastLength = Request->Command.Length;
    Extension->DiagLastTimeoutStage = 0;

    switch (Request->Type) {
    case SdRequestTypeCommandNoTransfer:
    case SdRequestTypeCommandWithTransfer:
        InterlockedIncrement(&Extension->DiagCommandPhaseCount);
        if (InterlockedCompareExchangePointer(
                &Extension->OutstandingRequest,
                Request,
                NULL) != NULL) {
            InterlockedIncrement(&Extension->DiagBusyRejectCount);
            Extension->DiagLastCompletionStatus =
                (ULONG)STATUS_DEVICE_BUSY;
            MtkMsdcQueueDiagWork(Extension);
            return STATUS_DEVICE_BUSY;
        }

        Status = MtkMsdcIssueCommand(Extension, Request);
        if (Status != STATUS_PENDING) {
            (VOID)InterlockedCompareExchangePointer(
                &Extension->OutstandingRequest,
                NULL,
                Request);
            Extension->DiagLastCompletionStatus = (ULONG)Status;
            if (Status == STATUS_IO_TIMEOUT) {
                Extension->DiagLastTimeoutStage = 1;
            }
            MtkMsdcQueueDiagWork(Extension);
        }
        return Status;

    case SdRequestTypeStartTransfer:
        InterlockedIncrement(&Extension->DiagStartTransferCount);
        /*
         * SDPORT presents StartTransfer as a new IssueRequest phase after the
         * command-with-transfer phase has been completed back to the port
         * driver.  Acquire fresh ownership for this phase; retaining command
         * ownership here prevents SDPORT from advancing the request.
         */
        if (InterlockedCompareExchangePointer(
                &Extension->OutstandingRequest,
                Request,
                NULL) != NULL) {
            InterlockedIncrement(&Extension->DiagBusyRejectCount);
            Extension->DiagLastCompletionStatus =
                (ULONG)STATUS_DEVICE_BUSY;
            MtkMsdcQueueDiagWork(Extension);
            return STATUS_DEVICE_BUSY;
        }
        Status = MtkMsdcStartTransfer(Extension, Request);
        Extension->DiagLastRawInterrupt = MtkMsdcRead(Extension, MSDC_INT);
        Extension->DiagLastIntEnable = MtkMsdcRead(Extension, MSDC_INTEN);
        Extension->DiagLastSdcStatus = MtkMsdcRead(Extension, SDC_STS);
        Extension->DiagLastFifoStatus = MtkMsdcRead(Extension, MSDC_FIFOCS);
        if (NT_SUCCESS(Status)) {
            /*
             * The bounded polling path has already observed the hardware's
             * transfer-complete condition.  Do not enable the interrupt after
             * the status bit is already latched: MTK is not required to emit a
             * new edge, which can strand this StartTransfer forever.  SDPORT's
             * own sample permits a StartTransfer implementation to complete
             * the request directly (its ADMA path does exactly this).
             */
            MtkMsdcWrite(Extension, MSDC_INT, MSDC_INT_DATA_STATUS);
            MtkMsdcClearBits(Extension, MSDC_INTEN, MSDC_INT_DATA_STATUS);
            Request->RequiredEvents = 0;
            Request->Status = STATUS_SUCCESS;
            Extension->DiagLastRequiredEvents = Request->RequiredEvents;
            Extension->DiagLastCompletionStatus = (ULONG)STATUS_SUCCESS;
            if (InterlockedCompareExchangePointer(
                    &Extension->OutstandingRequest,
                    NULL,
                    Request) == Request) {
                InterlockedIncrement(&Extension->DiagCompleteCount);
                MtkMsdcQueueDiagWork(Extension);
                SdPortCompleteRequest(Request, STATUS_SUCCESS);
                return STATUS_SUCCESS;
            }

            InterlockedIncrement(&Extension->DiagStaleDpcCount);
            MtkMsdcQueueDiagWork(Extension);
            return STATUS_DEVICE_PROTOCOL_ERROR;
        }

        Request->RequiredEvents = 0;
        Request->Status = Status;
        MtkMsdcClearBits(Extension, MSDC_INTEN, MSDC_INT_DATA_STATUS);
        (VOID)InterlockedCompareExchangePointer(
            &Extension->OutstandingRequest,
            NULL,
            Request);
        Extension->DiagLastCompletionStatus = (ULONG)Status;
        if (Status == STATUS_IO_TIMEOUT) {
            Extension->DiagLastTimeoutStage = 2;
        }
        MtkMsdcQueueDiagWork(Extension);
        return Status;

    default:
        return STATUS_NOT_SUPPORTED;
    }
}


/*
 * SD CRC7 (x^7 + x^3 + 1), MSB-first, init 0.  Protects the 120 payload
 * bits of an R2 response; the stored byte is CRC<<1 | end-bit.
 */
static UCHAR
MtkMsdcCrc7(
    const UCHAR *Data,
    ULONG Length
    )
{
    UCHAR Crc;
    ULONG Index;
    ULONG Bit;

    Crc = 0;
    for (Index = 0; Index < Length; Index += 1) {
        Crc ^= Data[Index];
        for (Bit = 0; Bit < 8; Bit += 1) {
            Crc = (UCHAR)((Crc & 0x80) ? ((Crc << 1) ^ 0x12) : (Crc << 1));
        }
    }
    return Crc;
}

/*
 * Validate one R2 response and, for a CSD, extract the self-described
 * geometry.  Everything is computed from the reversed (wire-order) image.
 */
static VOID
MtkMsdcValidateR2(
    PMTK_MSDC_EXTENSION Extension,
    UCHAR CommandIndex
    )
{
    UCHAR Reversed[SDPORT_MAX_RESPONSE_LENGTH];
    UCHAR StoredCrc;
    ULONG Index;

    for (Index = 0; Index < SDPORT_MAX_RESPONSE_LENGTH; Index += 1) {
        Reversed[Index] =
            ((const UCHAR *)Extension->Response)[SDPORT_MAX_RESPONSE_LENGTH - 1 - Index];
    }
    StoredCrc = (UCHAR)(Reversed[15] >> 1);

    if (CommandIndex == 2) {
        Extension->DiagCidCrcOk =
            (MtkMsdcCrc7(Reversed, 15) == StoredCrc) ? 1 : 0;
    } else if (CommandIndex == 9) {
        Extension->DiagCsdCrcOk =
            (MtkMsdcCrc7(Reversed, 15) == StoredCrc) ? 1 : 0;
        Extension->DiagCsdStructure = Reversed[0] >> 6;
        if (Extension->DiagCsdStructure <= 1) {
            ULONG ReadBlLen = Reversed[5] & 0x0F;
            ULONG CSize = ((ULONG)(Reversed[6] & 0x03) << 10) |
                          ((ULONG)Reversed[7] << 2) |
                          (Reversed[8] >> 6);
            ULONG_PTR Bytes = ((ULONG_PTR)(CSize + 1)) << (ReadBlLen + 2);
            Extension->DiagCsdCapKb = (ULONG)(Bytes >> 10);
        } else {
            ULONG CSize22 = ((ULONG)(Reversed[7] & 0x3F) << 16) |
                            ((ULONG)Reversed[8] << 8) |
                            Reversed[9];
            Extension->DiagCsdCapKb = (CSize22 + 1) * 512;
        }
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
     * Microsoft's SDPORT reference miniport (sdhc.c, SdhcGetResponse) copies
     * the response register space byte-by-byte in ascending order with no
     * swap, reorder, or compaction.  SDPORT expects the RAW controller
     * register image and performs its own wire-order assembly.
     *
     * SDHCI stores R2 payload bits 127:96 in RESP3 and 31:0 in RESP0 (CRC
     * bits dropped); MT6768 MSDC stores the same 128 payload bits the same
     * way (verified: the eMMC CID and a valid 128 GB SD CSD v2 both decode
     * from the raw image, first word in RESP3, MSB-first).  No transform is
     * needed here.  Both the one-byte compaction (0.10.0/0.13.0, physically
     * failed with sdstor loaded in M2.68-M2.71) and the full byte reversal
     * (0.12.0, failed in M2.72) corrupt the image SDPORT assembles.  The
     * 0.9.0 straight copy predates sdstor being loaded and was never
     * exercised end to end.
     */
    UNREFERENCED_PARAMETER(RawResponse);
    Extension->DiagGr2Index = Command->Index;
    Extension->DiagGr2Resp[0] = MtkMsdcRead(Extension, SDC_RESP0);
    Extension->DiagGr2Resp[1] = MtkMsdcRead(Extension, SDC_RESP1);
    Extension->DiagGr2Resp[2] = MtkMsdcRead(Extension, SDC_RESP2);
    Extension->DiagGr2Resp[3] = MtkMsdcRead(Extension, SDC_RESP3);
    RtlCopyMemory(ResponseBuffer,
                  Extension->Response,
                  sizeof(Extension->Response));
    if (Command->ResponseType == SdResponseTypeR2 &&
        Command->Index == 9 &&
        Extension->IsEmmc == FALSE &&
        (Extension->DiagOcrValue & 0x40000000) != 0) {
        PUCHAR Out = (PUCHAR)ResponseBuffer;
        /*
         * Wire response head repair.  In this register image the CSD's
         * structure byte (CSD[127:120]) is RESP3's most significant byte,
         * i.e. Out[15].  This card delivers 0x40 there instead of 0x00 -
         * a corrupted head bit that reclassifies the CSD as v1 with an
         * absurd 30 KB geometry, which sdstor correctly refuses.  A card
         * whose OCR reports CCS=1 is SDHC/SDXC/SDUC and must present a
         * CSD v2 (structure 00); clear the two structure bits so SDPORT
         * parses the true geometry (C_SIZE 0x03b8ab = 119.5 GiB).
         */
        if ((Out[15] >> 6) == 1) {
            Out[15] = (UCHAR)(Out[15] & 0x3F);
            Extension->DiagCsdRepaired = 1;
        }
    }
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
    PMTK_MSDC_EXTENSION Extension;
    NTSTATUS Status;

    if (PrivateExtension == NULL || Request == NULL) {
        return;
    }

    Extension = (PMTK_MSDC_EXTENSION)PrivateExtension;
    InterlockedIncrement(&Extension->DiagDpcCount);
    Extension->DiagLastEvents = Events;
    Extension->DiagLastErrors = Errors;
    Extension->DiagLastRequiredEvents = Request->RequiredEvents;
    if (InterlockedCompareExchangePointer(
            &Extension->OutstandingRequest,
            Request,
            Request) != Request) {
        InterlockedIncrement(&Extension->DiagStaleDpcCount);
        MtkMsdcQueueDiagWork(Extension);
        return;
    }

    Request->RequiredEvents &= ~Events;

    if (Errors != 0) {
        Request->RequiredEvents = 0;
        if ((Errors & (SDPORT_ERROR_CMD_TIMEOUT |
                       SDPORT_ERROR_DATA_TIMEOUT |
                       SDPORT_ERROR_ACMD12_RESPONSE_TIMEOUT)) != 0) {
            Status = STATUS_IO_TIMEOUT;
        } else if ((Errors & (SDPORT_ERROR_CMD_CRC_ERROR |
                              SDPORT_ERROR_DATA_CRC_ERROR |
                              SDPORT_ERROR_ACMD12_RESPONSE_CRC_ERROR)) != 0) {
            Status = STATUS_CRC_ERROR;
        } else {
            Status = STATUS_IO_DEVICE_ERROR;
        }
    } else {
        Status = STATUS_SUCCESS;
        if ((Events & SDPORT_EVENT_CARD_RESPONSE) != 0) {
            MtkMsdcCaptureResponse(Extension);
            if (Request->Command.Index == 41 &&
                Request->Command.ResponseType == SdResponseTypeR3) {
                Extension->DiagOcrValue = Extension->Response[0];
            }
            if (Request->Command.ResponseType == SdResponseTypeR2) {
                if (Request->Command.Index == 2) {
                    RtlCopyMemory(Extension->DiagCidResponse,
                                  Extension->Response,
                                  sizeof(Extension->DiagCidResponse));
                    MtkMsdcValidateR2(Extension, 2);
                } else if (Request->Command.Index == 9) {
                    RtlCopyMemory(Extension->DiagCsdResponse,
                                  Extension->Response,
                                  sizeof(Extension->DiagCsdResponse));
                    MtkMsdcValidateR2(Extension, 9);
                }
                if (Request->Command.Index == 2 ||
                    Request->Command.Index == 9) {
                    /*
                     * Latch-timing instrument: re-read the response registers
                     * at 25/100/300/1000 us.  The CMD2 capture has shown a
                     * 4-bit head shift in RESP3 that CRC proves wrong, so
                     * find when the registers hold the true bytes.
                     */
                    static const ULONG SnapDelayUs[4] = { 25, 100, 300, 1000 };
                    ULONG DelayIndex;
                    ULONG RegIndex;
                    for (DelayIndex = 0; DelayIndex < 4; DelayIndex += 1) {
                        SdPortWait(SnapDelayUs[DelayIndex]);
                        for (RegIndex = 0; RegIndex < 4; RegIndex += 1) {
                            Extension->DiagSnap[DelayIndex][RegIndex] =
                                MtkMsdcRead(Extension, SDC_RESP0 + RegIndex * 4);
                        }
                    }
                }
            }
            MtkMsdcClearBits(Extension, MSDC_INTEN, MSDC_INT_CMD_STATUS);
            if (Request->Command.ResponseType == SdResponseTypeR1B ||
                Request->Command.ResponseType == SdResponseTypeR5B) {
                Status = MtkMsdcWaitReady(Extension, TRUE);
            }
        }
    }

    if (!NT_SUCCESS(Status) || Request->RequiredEvents == 0) {
        MtkMsdcClearBits(Extension,
                         MSDC_INTEN,
                         MSDC_INT_CMD_STATUS | MSDC_INT_DATA_STATUS);
        Request->RequiredEvents = 0;
        if (!NT_SUCCESS(Status)) {
            Request->Status = Status;
        } else if (Request->Status != STATUS_MORE_PROCESSING_REQUIRED) {
            Request->Status = STATUS_SUCCESS;
        }
        if (InterlockedCompareExchangePointer(
                &Extension->OutstandingRequest,
                NULL,
                Request) == Request) {
            InterlockedIncrement(&Extension->DiagCompleteCount);
            Extension->DiagLastCompletionStatus =
                (ULONG)Request->Status;
            Extension->DiagLastRequiredEvents = Request->RequiredEvents;
            Extension->DiagLastRawInterrupt =
                MtkMsdcRead(Extension, MSDC_INT);
            Extension->DiagLastIntEnable =
                MtkMsdcRead(Extension, MSDC_INTEN);
            Extension->DiagLastSdcStatus =
                MtkMsdcRead(Extension, SDC_STS);
            Extension->DiagLastFifoStatus =
                MtkMsdcRead(Extension, MSDC_FIFOCS);
            if (Request->Status == STATUS_IO_TIMEOUT) {
                Extension->DiagLastTimeoutStage = 3;
            }
            MtkMsdcQueueDiagWork(Extension);
            SdPortCompleteRequest(Request, Request->Status);
        }
    }
}

static ULONG
MtkMsdcInterruptMaskFromEvents(
    _In_ ULONG EventMask
    )
{
    ULONG InterruptMask;

    InterruptMask = 0;
    if ((EventMask & SDPORT_EVENT_CARD_RESPONSE) != 0) {
        InterruptMask |= MSDC_INT_CMD_STATUS;
    }
    if ((EventMask & SDPORT_EVENT_CARD_RW_END) != 0) {
        InterruptMask |= MSDC_INT_DATA_STATUS;
    }
    if ((EventMask & SDPORT_EVENT_ERROR) != 0) {
        InterruptMask |= MSDC_INT_CMD_ERROR |
                         MSDC_INT_DATA_ERROR |
                         MSDC_INT_ACMD_ERROR;
    }
    if ((EventMask & SDPORT_EVENT_CARD_CHANGE) != 0) {
        InterruptMask |= MSDC_INT_CDSC;
    }
    if ((EventMask & SDPORT_EVENT_CARD_INTERRUPT) != 0) {
        InterruptMask |= MSDC_INT_MMCIRQ;
    }
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
