#pragma once

#include <ntddk.h>
#include <sdport.h>

#include "mtkmsdc_regs.h"

#define MTK_MSDC_TRACE_DEPTH 16

typedef struct _MTK_MSDC_EXTENSION {
    PHYSICAL_ADDRESS PhysicalBase;
    PVOID BaseAddress;
    ULONG BaseAddressLength;
    ULONG CurrentClockHz;
    ULONG Response[4];
    SDPORT_CAPABILITIES Capabilities;
    ULONG HostIndex;
    BOOLEAN IsEmmc;
    BOOLEAN CrashdumpMode;
    PVOID volatile OutstandingRequest;
    WORK_QUEUE_ITEM DiagWorkItem;
    volatile LONG DiagWorkQueued;
    volatile LONG DiagIssueCount;
    volatile LONG DiagIsrCount;
    volatile LONG DiagDpcCount;
    volatile LONG DiagCompleteCount;
    volatile LONG DiagCommandPhaseCount;
    volatile LONG DiagStartTransferCount;
    volatile LONG DiagBusyRejectCount;
    volatile LONG DiagStaleDpcCount;
    volatile LONG DiagCardDetectCount;
    volatile LONG DiagTraceSequence;
    ULONG DiagLastRequestType;
    ULONG DiagLastCommand;
    ULONG DiagLastArgument;
    ULONG DiagLastResponseType;
    ULONG DiagLastTransferType;
    ULONG DiagLastDirection;
    ULONG DiagLastBlockSize;
    ULONG DiagLastBlockCount;
    ULONG DiagLastLength;
    ULONG DiagLastRequiredEvents;
    ULONG DiagLastEvents;
    ULONG DiagLastErrors;
    ULONG DiagLastRawInterrupt;
    ULONG DiagLastIntEnable;
    ULONG DiagLastSdcStatus;
    ULONG DiagLastFifoStatus;
    ULONG DiagLastCompletionStatus;
    ULONG DiagLastTimeoutStage;
    ULONG DiagLastCardPresent;
    ULONG DiagCurrentBusWidth;
    ULONG DiagCidResponse[4];
    ULONG DiagCsdResponse[4];
    ULONG DiagCsdLateResponse[4];
    ULONG DiagCidCrcOk;
    ULONG DiagCsdCrcOk;
    ULONG DiagCsdStructure;
    ULONG DiagCsdCapKb;
    ULONG DiagSnap[4][4];
    ULONG DiagGr2Index;
    ULONG DiagGr2Resp[4];
    ULONG DiagOcrValue;
    ULONG DiagCsdRepaired;
    ULONG DiagTraceRequest[MTK_MSDC_TRACE_DEPTH];
    ULONG DiagTraceArgument[MTK_MSDC_TRACE_DEPTH];
} MTK_MSDC_EXTENSION, *PMTK_MSDC_EXTENSION;

DRIVER_INITIALIZE DriverEntry;

SDPORT_GET_SLOT_COUNT MtkMsdcGetSlotCount;
SDPORT_GET_SLOT_CAPABILITIES MtkMsdcGetSlotCapabilities;
SDPORT_INITIALIZE MtkMsdcInitialize;
SDPORT_ISSUE_BUS_OPERATION MtkMsdcIssueBusOperation;
SDPORT_GET_CARD_DETECT_STATE MtkMsdcGetCardDetectState;
SDPORT_GET_WRITE_PROTECT_STATE MtkMsdcGetWriteProtectState;
SDPORT_INTERRUPT MtkMsdcInterrupt;
SDPORT_ISSUE_REQUEST MtkMsdcIssueRequest;
SDPORT_GET_RESPONSE MtkMsdcGetResponse;
SDPORT_REQUEST_DPC MtkMsdcRequestDpc;
SDPORT_TOGGLE_EVENTS MtkMsdcToggleEvents;
SDPORT_CLEAR_EVENTS MtkMsdcClearEvents;
SDPORT_SAVE_CONTEXT MtkMsdcSaveContext;
SDPORT_RESTORE_CONTEXT MtkMsdcRestoreContext;
SDPORT_PO_FX_POWER_CONTROL_CALLBACK MtkMsdcPowerControl;
SDPORT_CLEANUP MtkMsdcCleanup;
