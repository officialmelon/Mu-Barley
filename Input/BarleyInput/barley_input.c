/*
 * Lenovo TB330XU (Barley) physical-key HID transport.
 *
 * This is a KMDF HID minidriver using Microsoft's MsHidKmdf transport.  It is
 * deliberately ACPI-resource driven: physical bases are supplied by BAR0001
 * and are never compiled into this driver.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "barley_input.h"

static HID_REPORT_DESCRIPTOR gBarleyReportDescriptor[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x05, 0x07,       /*   Usage Page (Keyboard/Keypad) */
    0x19, 0xE0,       /*   Usage Minimum (Left Control) */
    0x29, 0xE7,       /*   Usage Maximum (Right GUI) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x01,       /*   Logical Maximum (1) */
    0x75, 0x01,       /*   Report Size (1) */
    0x95, 0x08,       /*   Report Count (8) */
    0x81, 0x02,       /*   Input (Data, Variable, Absolute) */
    0x95, 0x01,       /*   Report Count (1) */
    0x75, 0x08,       /*   Report Size (8) */
    0x81, 0x01,       /*   Input (Constant) */
    0x95, 0x06,       /*   Report Count (6) */
    0x75, 0x08,       /*   Report Size (8) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x65,       /*   Logical Maximum (101) */
    0x19, 0x00,       /*   Usage Minimum (Reserved) */
    0x29, 0x65,       /*   Usage Maximum (Keyboard Application) */
    0x81, 0x00,       /*   Input (Data, Array, Absolute) */
    0xC0              /* End Collection */
};

static HID_DESCRIPTOR gBarleyHidDescriptor = {
    sizeof(HID_DESCRIPTOR),
    HID_HID_DESCRIPTOR_TYPE,
    0x0111,
    0,
    1,
    { { HID_REPORT_DESCRIPTOR_TYPE, sizeof(gBarleyReportDescriptor) } }
};

static HID_DEVICE_ATTRIBUTES gBarleyAttributes = {
    sizeof(HID_DEVICE_ATTRIBUTES),
    0x17EF, /* Lenovo */
    0x330B, /* TB330XU Barley physical keys */
    0x0001,
    { 0 }
};

static NTSTATUS
BarleyCopyToRequest(
    _In_ WDFREQUEST Request,
    _In_reads_bytes_(Length) const VOID *Source,
    _In_ size_t Length
    )
{
    NTSTATUS status;
    WDFMEMORY memory;
    size_t destinationLength;

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    (void)WdfMemoryGetBuffer(memory, &destinationLength);
    if (destinationLength < Length) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0, (PVOID)Source, Length);
    if (NT_SUCCESS(status)) {
        WdfRequestSetInformation(Request, Length);
    }

    return status;
}

static ULONG
BarleyWacsFsm(
    _In_ PBARLEY_DEVICE_CONTEXT Context
    )
{
    ULONG value;

    value = READ_REGISTER_ULONG(
        (PULONG)(Context->PwrapRegisters + BARLEY_PWRAP_WACS2_RDATA_OFFSET));
    return (value >> 16) & 0x7U;
}

static NTSTATUS
BarleyWaitForWacsFsm(
    _In_ PBARLEY_DEVICE_CONTEXT Context,
    _In_ ULONG ExpectedFsm
    )
{
    ULONG poll;

    for (poll = 0; poll < BARLEY_WACS_MAX_POLLS; ++poll) {
        if (BarleyWacsFsm(Context) == ExpectedFsm) {
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(BARLEY_WACS_POLL_DELAY_US);
    }

    return STATUS_IO_TIMEOUT;
}

static NTSTATUS
BarleyPmicRead16(
    _In_ PBARLEY_DEVICE_CONTEXT Context,
    _In_ USHORT Address,
    _Out_ PUSHORT Value
    )
{
    NTSTATUS status;
    ULONG result;

    if (!Context->PwrapReady || Value == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    /* Recover only the documented stale-result state before a new command. */
    if (BarleyWacsFsm(Context) == BARLEY_WACS_FSM_WFVLDCLR) {
        WRITE_REGISTER_ULONG(
            (PULONG)(Context->PwrapRegisters + BARLEY_PWRAP_WACS2_VLDCLR_OFFSET),
            1U);
    }

    status = BarleyWaitForWacsFsm(Context, BARLEY_WACS_FSM_IDLE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* MT6768 non-arbitrated WACS read: RW=0, address is half-word indexed. */
    WRITE_REGISTER_ULONG(
        (PULONG)(Context->PwrapRegisters + BARLEY_PWRAP_WACS2_CMD_OFFSET),
        ((ULONG)(Address >> 1)) << 16);

    status = BarleyWaitForWacsFsm(Context, BARLEY_WACS_FSM_WFVLDCLR);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    result = READ_REGISTER_ULONG(
        (PULONG)(Context->PwrapRegisters + BARLEY_PWRAP_WACS2_RDATA_OFFSET));
    *Value = (USHORT)(result & 0xFFFFU);

    WRITE_REGISTER_ULONG(
        (PULONG)(Context->PwrapRegisters + BARLEY_PWRAP_WACS2_VLDCLR_OFFSET),
        1U);

    return BarleyWaitForWacsFsm(Context, BARLEY_WACS_FSM_IDLE);
}

static VOID
BarleyAppendUsage(
    _Inout_ PBARLEY_KEYBOARD_REPORT Report,
    _In_ UCHAR Usage
    )
{
    ULONG index;

    if (Usage == 0) {
        return;
    }

    for (index = 0; index < RTL_NUMBER_OF(Report->Keys); ++index) {
        if (Report->Keys[index] == Usage) {
            return;
        }
        if (Report->Keys[index] == 0) {
            Report->Keys[index] = Usage;
            return;
        }
    }
}

static VOID
BarleySampleKeys(
    _Inout_ PBARLEY_DEVICE_CONTEXT Context
    )
{
    BARLEY_KEYBOARD_REPORT report;
    USHORT topStatus;
    USHORT kpdMem1;
    BOOLEAN tabDown;
    BOOLEAN homeDown;
    BOOLEAN powerDown;

    RtlZeroMemory(&report, sizeof(report));

    kpdMem1 = READ_REGISTER_USHORT(
        (PUSHORT)(Context->KpdRegisters + BARLEY_KPD_MEM1_OFFSET));
    tabDown = (kpdMem1 & 1U) == 0;

    homeDown = FALSE;
    powerDown = FALSE;
    if (NT_SUCCESS(BarleyPmicRead16(Context, BARLEY_PMIC_TOPSTATUS, &topStatus))) {
        homeDown = (topStatus & BARLEY_PMIC_HOME_MASK) == 0;
        powerDown = (topStatus & BARLEY_PMIC_PWRKEY_MASK) == 0;
    }

    if (tabDown || homeDown) {
        BarleyAppendUsage(&report, BARLEY_HID_USAGE_TAB);
    }
    if (homeDown) {
        report.Modifiers |= BARLEY_HID_MODIFIER_LEFT_SHIFT;
    }

    if (powerDown) {
        if (!Context->PowerWasDown) {
            Context->PowerHoldTicks = 0;
            Context->PowerLongSent = FALSE;
        } else if (Context->PowerHoldTicks != MAXULONG) {
            ++Context->PowerHoldTicks;
        }

        if (Context->PowerHoldTicks >= BARLEY_POWER_LONG_PRESS_TICKS) {
            Context->PowerLongSent = TRUE;
            BarleyAppendUsage(&report, BARLEY_HID_USAGE_ESCAPE);
        }
    } else if (Context->PowerWasDown) {
        if (!Context->PowerLongSent) {
            Context->TapEnterTicks = 1;
        }
        Context->PowerHoldTicks = 0;
        Context->PowerLongSent = FALSE;
    }

    Context->PowerWasDown = powerDown;

    if (Context->TapEnterTicks != 0) {
        BarleyAppendUsage(&report, BARLEY_HID_USAGE_ENTER);
        --Context->TapEnterTicks;
    }

    Context->CurrentReport = report;
}

static VOID
BarleyCompleteChangedReport(
    _Inout_ PBARLEY_DEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    WDFREQUEST request;

    if (Context->HaveLastReport &&
        RtlCompareMemory(
            &Context->CurrentReport,
            &Context->LastReported,
            sizeof(Context->CurrentReport)) == sizeof(Context->CurrentReport)) {
        return;
    }

    status = WdfIoQueueRetrieveNextRequest(Context->ManualReadQueue, &request);
    if (!NT_SUCCESS(status)) {
        return;
    }

    status = BarleyCopyToRequest(
        request,
        &Context->CurrentReport,
        sizeof(Context->CurrentReport));
    if (NT_SUCCESS(status)) {
        Context->LastReported = Context->CurrentReport;
        Context->HaveLastReport = TRUE;
    }
    WdfRequestComplete(request, status);
}

VOID
BarleyEvtPollTimer(
    _In_ WDFTIMER Timer
    )
{
    WDFDEVICE device;
    PBARLEY_DEVICE_CONTEXT context;

    device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    context = BarleyGetContext(device);
    BarleySampleKeys(context);
    BarleyCompleteChangedReport(context);
}

static NTSTATUS
BarleyCreateQueuesAndTimer(
    _In_ WDFDEVICE Device
    )
{
    NTSTATUS status;
    PBARLEY_DEVICE_CONTEXT context;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;

    context = BarleyGetContext(Device);

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel);
    queueConfig.EvtIoInternalDeviceControl = BarleyEvtIoInternalDeviceControl;
    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &context->ManualReadQueue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_TIMER_CONFIG_INIT_PERIODIC(
        &timerConfig,
        BarleyEvtPollTimer,
        BARLEY_POLL_PERIOD_MS);
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = Device;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    return WdfTimerCreate(&timerConfig, &timerAttributes, &context->PollTimer);
}

VOID
BarleyEvtIoInternalDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    NTSTATUS status;
    BOOLEAN completeRequest;
    WDFDEVICE device;
    PBARLEY_DEVICE_CONTEXT context;
    PIRP irp;
    PHID_XFER_PACKET packet;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    completeRequest = TRUE;
    device = WdfIoQueueGetDevice(Queue);
    context = BarleyGetContext(device);

    switch (IoControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        status = BarleyCopyToRequest(
            Request,
            &gBarleyHidDescriptor,
            sizeof(gBarleyHidDescriptor));
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        status = BarleyCopyToRequest(
            Request,
            gBarleyReportDescriptor,
            sizeof(gBarleyReportDescriptor));
        break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        status = BarleyCopyToRequest(
            Request,
            &gBarleyAttributes,
            sizeof(gBarleyAttributes));
        break;

    case IOCTL_HID_READ_REPORT:
        status = WdfRequestForwardToIoQueue(Request, context->ManualReadQueue);
        if (NT_SUCCESS(status)) {
            completeRequest = FALSE;
        }
        break;

    case IOCTL_HID_GET_INPUT_REPORT:
        irp = WdfRequestWdmGetIrp(Request);
        packet = (PHID_XFER_PACKET)irp->UserBuffer;
        if (packet == NULL ||
            packet->reportBuffer == NULL ||
            packet->reportBufferLen < sizeof(context->CurrentReport)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        RtlCopyMemory(
            packet->reportBuffer,
            &context->CurrentReport,
            sizeof(context->CurrentReport));
        WdfRequestSetInformation(Request, sizeof(context->CurrentReport));
        status = STATUS_SUCCESS;
        break;

    case IOCTL_HID_ACTIVATE_DEVICE:
    case IOCTL_HID_DEACTIVATE_DEVICE:
        status = STATUS_SUCCESS;
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (completeRequest) {
        WdfRequestComplete(Request, status);
    }
}

NTSTATUS
BarleyEvtPrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    NTSTATUS status;
    PBARLEY_DEVICE_CONTEXT context;
    ULONG count;
    ULONG index;
    ULONG memoryIndex;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    context = BarleyGetContext(Device);
    memoryIndex = 0;
    count = WdfCmResourceListGetCount(ResourcesTranslated);

    for (index = 0; index < count; ++index) {
        descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, index);
        if (descriptor == NULL || descriptor->Type != CmResourceTypeMemory) {
            continue;
        }

        if (memoryIndex == 0) {
            context->KpdLength = descriptor->u.Memory.Length;
            if (context->KpdLength < BARLEY_KPD_MEM1_OFFSET + sizeof(USHORT)) {
                status = STATUS_DEVICE_CONFIGURATION_ERROR;
                goto Failure;
            }
            context->KpdRegisters = (PUCHAR)MmMapIoSpace(
                descriptor->u.Memory.Start,
                context->KpdLength,
                MmNonCached);
            if (context->KpdRegisters == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto Failure;
            }
        } else if (memoryIndex == 1) {
            context->PwrapLength = descriptor->u.Memory.Length;
            if (context->PwrapLength < BARLEY_PWRAP_WACS2_VLDCLR_OFFSET + sizeof(ULONG)) {
                status = STATUS_DEVICE_CONFIGURATION_ERROR;
                goto Failure;
            }
            context->PwrapRegisters = (PUCHAR)MmMapIoSpace(
                descriptor->u.Memory.Start,
                context->PwrapLength,
                MmNonCached);
            if (context->PwrapRegisters == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto Failure;
            }
        }

        ++memoryIndex;
    }

    if (context->KpdRegisters == NULL || context->PwrapRegisters == NULL) {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        goto Failure;
    }

    return STATUS_SUCCESS;

Failure:
    if (context->PwrapRegisters != NULL) {
        MmUnmapIoSpace(context->PwrapRegisters, context->PwrapLength);
        context->PwrapRegisters = NULL;
        context->PwrapLength = 0;
    }
    if (context->KpdRegisters != NULL) {
        MmUnmapIoSpace(context->KpdRegisters, context->KpdLength);
        context->KpdRegisters = NULL;
        context->KpdLength = 0;
    }
    return status;
}

NTSTATUS
BarleyEvtReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PBARLEY_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(ResourcesTranslated);
    context = BarleyGetContext(Device);

    context->PwrapReady = FALSE;
    if (context->PwrapRegisters != NULL) {
        MmUnmapIoSpace(context->PwrapRegisters, context->PwrapLength);
        context->PwrapRegisters = NULL;
        context->PwrapLength = 0;
    }
    if (context->KpdRegisters != NULL) {
        MmUnmapIoSpace(context->KpdRegisters, context->KpdLength);
        context->KpdRegisters = NULL;
        context->KpdLength = 0;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
BarleyEvtD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    PBARLEY_DEVICE_CONTEXT context;
    ULONG initDone;

    UNREFERENCED_PARAMETER(PreviousState);
    context = BarleyGetContext(Device);

    RtlZeroMemory(&context->CurrentReport, sizeof(context->CurrentReport));
    RtlZeroMemory(&context->LastReported, sizeof(context->LastReported));
    context->HaveLastReport = FALSE;
    context->PowerWasDown = FALSE;
    context->PowerLongSent = FALSE;
    context->PowerHoldTicks = 0;
    context->TapEnterTicks = 0;

    initDone = READ_REGISTER_ULONG(
        (PULONG)(context->PwrapRegisters + BARLEY_PWRAP_INIT_DONE2_OFFSET));
    context->PwrapReady = initDone == 1U;

    WdfTimerStart(context->PollTimer, WDF_REL_TIMEOUT_IN_MS(1));
    return STATUS_SUCCESS;
}

NTSTATUS
BarleyEvtD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    PBARLEY_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(TargetState);
    context = BarleyGetContext(Device);
    (void)WdfTimerStop(context->PollTimer, TRUE);
    context->PwrapReady = FALSE;
    return STATUS_SUCCESS;
}

NTSTATUS
BarleyEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PBARLEY_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(Driver);

    /* MsHidKmdf owns the HID FDO; this hardware transport is its lower filter. */
    WdfFdoInitSetFilter(DeviceInit);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = BarleyEvtPrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = BarleyEvtReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = BarleyEvtD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = BarleyEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BARLEY_DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    context = BarleyGetContext(device);
    RtlZeroMemory(context, sizeof(*context));
    context->Device = device;

    return BarleyCreateQueuesAndTimer(device);
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);
    WDF_DRIVER_CONFIG_INIT(&config, BarleyEvtDeviceAdd);
    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE);
}
