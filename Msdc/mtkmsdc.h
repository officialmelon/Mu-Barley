#pragma once

#include <ntddk.h>
#include <sdport.h>

#include "mtkmsdc_regs.h"

typedef struct _MTK_MSDC_EXTENSION {
    PHYSICAL_ADDRESS PhysicalBase;
    PVOID BaseAddress;
    ULONG BaseAddressLength;
    ULONG CurrentClockHz;
    ULONG Response[4];
    SDPORT_CAPABILITIES Capabilities;
    BOOLEAN CrashdumpMode;
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

