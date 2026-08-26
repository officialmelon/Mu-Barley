/*
 * Lenovo TB330XU (Barley) HX83102J Windows touchscreen source driver.
 *
 * The hardware-facing transport is kept separate from the Himax protocol and
 * the Windows HID/VHF frontend.  This first implementation polls the real
 * MT6768 SPI0 controller in PIO mode; it does not fabricate an HID-over-I2C
 * or HID-over-SPI device that the hardware does not implement.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "barley_touch.h"

#define BARLEY_TOUCH_VID 0x17EFU
#define BARLEY_TOUCH_PID 0x330CU

static VOID
BarleyWriteDiag(
    _In_ PCWSTR ValueName,
    ULONG Status
    );

/*
 * One fixed parallel contact record.  Windows requires Tip Switch, Contact
 * Identifier, X, Y, frame Contact Count, and Contact Count Maximum.  In Range
 * and Confidence are included because the native controller provides valid
 * coordinates rather than a separate confidence score.
 */
#define BARLEY_TOUCH_FINGER_COLLECTION                                      \
    0x05, 0x0D,             /* Usage Page (Digitizers) */                  \
    0x09, 0x22,             /* Usage (Finger) */                           \
    0xA1, 0x02,             /* Collection (Logical) */                    \
    0x09, 0x42,             /*   Usage (Tip Switch) */                     \
    0x09, 0x32,             /*   Usage (In Range) */                       \
    0x09, 0x47,             /*   Usage (Confidence) */                     \
    0x15, 0x00,             /*   Logical Minimum (0) */                    \
    0x25, 0x01,             /*   Logical Maximum (1) */                    \
    0x75, 0x01,             /*   Report Size (1) */                        \
    0x95, 0x03,             /*   Report Count (3) */                       \
    0x81, 0x02,             /*   Input (Data,Var,Abs) */                   \
    0x95, 0x05,             /*   Report Count (5) */                       \
    0x81, 0x03,             /*   Input (Const,Var,Abs) */                  \
    0x75, 0x08,             /*   Report Size (8) */                        \
    0x95, 0x01,             /*   Report Count (1) */                       \
    0x09, 0x51,             /*   Usage (Contact Identifier) */             \
    0x25, 0x09,             /*   Logical Maximum (9) */                    \
    0x81, 0x02,             /*   Input (Data,Var,Abs) */                   \
    0x05, 0x01,             /*   Usage Page (Generic Desktop) */           \
    0x15, 0x00,             /*   Logical Minimum (0) */                    \
    0x26, 0xDF, 0x2E,       /*   Logical Maximum X (11999) */              \
    0x75, 0x10,             /*   Report Size (16) */                       \
    0x95, 0x01,             /*   Report Count (1) */                       \
    0x55, 0x0E,             /*   Unit Exponent (-2) */                     \
    0x65, 0x13,             /*   Unit (Inch, English Linear) */             \
    0x35, 0x00,             /*   Physical Minimum (0) */                   \
    0x46, 0x44, 0x02,       /*   Physical Maximum X (5.80 inches) */        \
    0x09, 0x30,             /*   Usage (X) */                              \
    0x81, 0x02,             /*   Input (Data,Var,Abs) */                   \
    0x26, 0xFF, 0x4A,       /*   Logical Maximum Y (19199) */              \
    0x46, 0xA1, 0x03,       /*   Physical Maximum Y (9.29 inches) */        \
    0x09, 0x31,             /*   Usage (Y) */                              \
    0x81, 0x02,             /*   Input (Data,Var,Abs) */                   \
    0xC0                    /* End Collection */

static UCHAR gBarleyTouchReportDescriptor[] = {
    0x05, 0x0D,             /* Usage Page (Digitizers) */
    0x09, 0x04,             /* Usage (Touch Screen) */
    0xA1, 0x01,             /* Collection (Application) */
    0x85, BARLEY_TOUCH_REPORT_ID,

    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,
    BARLEY_TOUCH_FINGER_COLLECTION,

    0x05, 0x0D,             /* Usage Page (Digitizers) */
    0x09, 0x54,             /* Usage (Contact Count) */
    0x15, 0x00,             /* Logical Minimum (0) */
    0x25, 0x0A,             /* Logical Maximum (10) */
    0x75, 0x08,             /* Report Size (8) */
    0x95, 0x01,             /* Report Count (1) */
    0x81, 0x02,             /* Input (Data,Var,Abs) */

    0x85, BARLEY_TOUCH_FEATURE_REPORT_ID,
    0x09, 0x55,             /* Usage (Contact Count Maximum) */
    0x15, 0x00,             /* Logical Minimum (0) */
    0x25, 0x0A,             /* Logical Maximum (10) */
    0x75, 0x08,             /* Report Size (8) */
    0x95, 0x01,             /* Report Count (1) */
    0xB1, 0x02,             /* Feature (Data,Var,Abs) */
    0xC0                    /* End Collection */
};

C_ASSERT(sizeof(BARLEY_TOUCH_CONTACT_REPORT) == 6U);
C_ASSERT(sizeof(BARLEY_TOUCH_INPUT_REPORT) == 62U);
C_ASSERT(sizeof(BARLEY_TOUCH_FEATURE_REPORT) == 2U);

static VOID
BarleyTouchUnmapResources(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    )
{
    if (Context->Spi.Gpio != NULL) {
        MmUnmapIoSpace(Context->Spi.Gpio, Context->Spi.GpioLength);
        Context->Spi.Gpio = NULL;
        Context->Spi.GpioLength = 0U;
    }
    if (Context->Spi.InfraCfg != NULL) {
        MmUnmapIoSpace(Context->Spi.InfraCfg, Context->Spi.InfraCfgLength);
        Context->Spi.InfraCfg = NULL;
        Context->Spi.InfraCfgLength = 0U;
    }
    if (Context->Spi.TopCkgen != NULL) {
        MmUnmapIoSpace(Context->Spi.TopCkgen, Context->Spi.TopCkgenLength);
        Context->Spi.TopCkgen = NULL;
        Context->Spi.TopCkgenLength = 0U;
    }
    if (Context->Spi.Registers != NULL) {
        MmUnmapIoSpace(Context->Spi.Registers, Context->Spi.Length);
        Context->Spi.Registers = NULL;
        Context->Spi.Length = 0U;
    }
}

static NTSTATUS
BarleyTouchMapMemoryResource(
    _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor,
    _Out_ PUCHAR *Base,
    _Out_ PULONG Length
    )
{
    if (Descriptor == NULL || Base == NULL || Length == NULL ||
        Descriptor->Type != CmResourceTypeMemory ||
        Descriptor->u.Memory.Length < 0x1000U) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    *Length = Descriptor->u.Memory.Length;
    *Base = (PUCHAR)MmMapIoSpace(
        Descriptor->u.Memory.Start,
        Descriptor->u.Memory.Length,
        MmNonCached);
    if (*Base == NULL) {
        *Length = 0U;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    return STATUS_SUCCESS;
}

VOID
BarleyTouchEvtVhfGetFeature(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
    )
{
    NTSTATUS status;
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(VhfOperationContext);
    context = (PBARLEY_TOUCH_DEVICE_CONTEXT)VhfClientContext;
    status = STATUS_INVALID_DEVICE_REQUEST;

    if (context != NULL && HidTransferPacket != NULL &&
        HidTransferPacket->reportId == BARLEY_TOUCH_FEATURE_REPORT_ID &&
        HidTransferPacket->reportBuffer != NULL &&
        HidTransferPacket->reportBufferLen >=
            sizeof(BARLEY_TOUCH_FEATURE_REPORT)) {
        HidTransferPacket->reportBuffer[0] =
            BARLEY_TOUCH_FEATURE_REPORT_ID;
        HidTransferPacket->reportBuffer[1] =
            (UCHAR)BARLEY_TOUCH_MAX_CONTACTS;
        status = STATUS_SUCCESS;
    }

    (void)VhfAsyncOperationComplete(VhfOperationHandle, status);
}

VOID
BarleyTouchEvtPollTimer(
    _In_ WDFTIMER Timer
    )
{
    NTSTATUS status;
    WDFDEVICE device;
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    context = BarleyTouchGetContext(device);
    if (!context->HardwareReady || context->VhfHandle == NULL) {
        goto Exit;
    }

    status = BarleyHimaxReadTouchReport(context, &context->CurrentReport);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }
    if (context->HaveLastReport &&
        RtlCompareMemory(
            &context->CurrentReport,
            &context->LastReport,
            sizeof(context->CurrentReport)) == sizeof(context->CurrentReport)) {
        goto Exit;
    }

    context->VhfInputPacket.reportBuffer =
        (PUCHAR)&context->CurrentReport;
    context->VhfInputPacket.reportBufferLen =
        sizeof(context->CurrentReport);
    context->VhfInputPacket.reportId = BARLEY_TOUCH_REPORT_ID;
    status = VhfReadReportSubmit(
        context->VhfHandle,
        &context->VhfInputPacket);
    if (NT_SUCCESS(status)) {
        context->LastReport = context->CurrentReport;
        context->HaveLastReport = TRUE;
    }

Exit:
    if (context->HardwareReady) {
        WdfTimerStart(
            context->PollTimer,
            WDF_REL_TIMEOUT_IN_MS(BARLEY_TOUCH_POLL_PERIOD_MS));
    }
}

NTSTATUS
BarleyTouchEvtPrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    NTSTATUS status;
    PBARLEY_TOUCH_DEVICE_CONTEXT context;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    ULONG count;
    ULONG index;
    ULONG memoryIndex;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    context = BarleyTouchGetContext(Device);
    memoryIndex = 0U;
    count = WdfCmResourceListGetCount(ResourcesTranslated);
    BarleyWriteDiag(L"DiagPrepareEntered", 0U);
    BarleyWriteDiag(L"DiagResourceCount", count);

    for (index = 0U; index < count; ++index) {
        descriptor = WdfCmResourceListGetDescriptor(
            ResourcesTranslated,
            index);
        if (descriptor == NULL || descriptor->Type != CmResourceTypeMemory) {
            continue;
        }

        switch (memoryIndex) {
        case BarleyTouchResourceSpi:
            status = BarleyTouchMapMemoryResource(
                descriptor,
                &context->Spi.Registers,
                &context->Spi.Length);
            break;
        case BarleyTouchResourceTopCkgen:
            status = BarleyTouchMapMemoryResource(
                descriptor,
                &context->Spi.TopCkgen,
                &context->Spi.TopCkgenLength);
            break;
        case BarleyTouchResourceInfraCfg:
            status = BarleyTouchMapMemoryResource(
                descriptor,
                &context->Spi.InfraCfg,
                &context->Spi.InfraCfgLength);
            break;
        case BarleyTouchResourceGpio:
            status = BarleyTouchMapMemoryResource(
                descriptor,
                &context->Spi.Gpio,
                &context->Spi.GpioLength);
            break;
        default:
            status = STATUS_DEVICE_CONFIGURATION_ERROR;
            break;
        }
        if (!NT_SUCCESS(status)) {
            BarleyWriteDiag(L"DiagPrepareStatus", status);
            BarleyTouchUnmapResources(context);
            return status;
        }
        ++memoryIndex;
    }

    if (memoryIndex != BarleyTouchResourceCount ||
        context->Spi.Registers == NULL || context->Spi.TopCkgen == NULL ||
        context->Spi.InfraCfg == NULL || context->Spi.Gpio == NULL) {
        BarleyWriteDiag(L"DiagMemoryCount", memoryIndex);
        BarleyWriteDiag(
            L"DiagPrepareStatus",
            STATUS_DEVICE_CONFIGURATION_ERROR);
        BarleyTouchUnmapResources(context);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    BarleyWriteDiag(L"DiagMemoryCount", memoryIndex);
    BarleyWriteDiag(L"DiagPrepareStatus", STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS
BarleyTouchEvtReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(ResourcesTranslated);
    context = BarleyTouchGetContext(Device);
    context->HardwareReady = FALSE;
    BarleyHimaxReleaseFirmware(context);
    BarleyTouchUnmapResources(context);
    return STATUS_SUCCESS;
}

NTSTATUS
BarleyTouchEvtD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    NTSTATUS status;
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(PreviousState);
    context = BarleyTouchGetContext(Device);
    BarleyWriteDiag(L"DiagD0Entered", 0U);
    context->HardwareReady = FALSE;
    context->HaveLastReport = FALSE;
    RtlZeroMemory(&context->CurrentReport, sizeof(context->CurrentReport));
    RtlZeroMemory(&context->LastReport, sizeof(context->LastReport));

    status = BarleyMtkSpiInitialize(&context->Spi);
    BarleyWriteDiag(L"DiagSpiInitStatus", status);
    if (!NT_SUCCESS(status)) {
        context->InitializationStage = 0x100U;
        context->InitializationStatus = status;
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "BarleyHimaxTouch: SPI initialization failed 0x%08X\n",
            status));
        return status;
    }

    status = BarleyHimaxInitialize(context);
    BarleyWriteDiag(L"DiagHimaxInitStatus", status);
    BarleyWriteDiag(L"DiagHimaxStage", context->InitializationStage);
    BarleyWriteDiag(L"DiagHimaxStageStatus", context->InitializationStatus);
    if (!NT_SUCCESS(status)) {
        BarleyHimaxReleaseFirmware(context);
        return status;
    }

    context->HardwareReady = TRUE;
    WdfTimerStart(context->PollTimer, WDF_REL_TIMEOUT_IN_MS(1));
    return STATUS_SUCCESS;
}

NTSTATUS
BarleyTouchEvtD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(TargetState);
    context = BarleyTouchGetContext(Device);
    context->HardwareReady = FALSE;
    (void)WdfTimerStop(context->PollTimer, TRUE);
    BarleyHimaxReleaseFirmware(context);
    return STATUS_SUCCESS;
}

VOID
BarleyTouchEvtDeviceCleanup(
    _In_ WDFOBJECT DeviceObject
    )
{
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    context = BarleyTouchGetContext(DeviceObject);
    context->HardwareReady = FALSE;
    if (context->PollTimer != NULL) {
        (void)WdfTimerStop(context->PollTimer, TRUE);
    }
    if (context->VhfHandle != NULL) {
        VhfDelete(context->VhfHandle, TRUE);
        context->VhfHandle = NULL;
    }
    BarleyHimaxReleaseFirmware(context);
}

static NTSTATUS
BarleyTouchCreateTimer(
    _In_ WDFDEVICE Device
    )
{
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    PBARLEY_TOUCH_DEVICE_CONTEXT context;

    context = BarleyTouchGetContext(Device);
    WDF_TIMER_CONFIG_INIT(&timerConfig, BarleyTouchEvtPollTimer);
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = Device;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    return WdfTimerCreate(
        &timerConfig,
        &timerAttributes,
        &context->PollTimer);
}

NTSTATUS
BarleyTouchEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PBARLEY_TOUCH_DEVICE_CONTEXT context;
    VHF_CONFIG vhfConfig;

    UNREFERENCED_PARAMETER(Driver);
    BarleyWriteDiag(L"DiagDeviceAddEntered", 0U);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware =
        BarleyTouchEvtPrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware =
        BarleyTouchEvtReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = BarleyTouchEvtD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = BarleyTouchEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &attributes,
        BARLEY_TOUCH_DEVICE_CONTEXT);
    attributes.EvtCleanupCallback = BarleyTouchEvtDeviceCleanup;
    attributes.ExecutionLevel = WdfExecutionLevelPassive;
    attributes.SynchronizationScope = WdfSynchronizationScopeDevice;
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    BarleyWriteDiag(L"DiagDeviceCreateStatus", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    context = BarleyTouchGetContext(device);
    context->Device = device;
    status = BarleyTouchCreateTimer(device);
    BarleyWriteDiag(L"DiagTimerCreateStatus", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    VHF_CONFIG_INIT(
        &vhfConfig,
        WdfDeviceWdmGetDeviceObject(device),
        (USHORT)sizeof(gBarleyTouchReportDescriptor),
        gBarleyTouchReportDescriptor);
    vhfConfig.VhfClientContext = context;
    vhfConfig.VendorID = BARLEY_TOUCH_VID;
    vhfConfig.ProductID = BARLEY_TOUCH_PID;
    vhfConfig.VersionNumber = 0x0001U;
    vhfConfig.EvtVhfAsyncOperationGetFeature =
        BarleyTouchEvtVhfGetFeature;

    status = VhfCreate(&vhfConfig, &context->VhfHandle);
    BarleyWriteDiag(L"DiagVhfCreateStatus", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = VhfStart(context->VhfHandle);
    BarleyWriteDiag(L"DiagVhfStartStatus", status);
    if (!NT_SUCCESS(status)) {
        VhfDelete(context->VhfHandle, TRUE);
        context->VhfHandle = NULL;
        return status;
    }
    context->VhfStarted = TRUE;
    BarleyWriteDiag(L"DiagDeviceAddComplete", 0U);
    return STATUS_SUCCESS;
}

static VOID
BarleyWriteDiag(
    _In_ PCWSTR ValueName,
    ULONG Status
    )
{
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING keyPath;
    UNICODE_STRING value;
    HANDLE key;

    RtlInitUnicodeString(&keyPath,
        L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\BarleyHimaxTouch");
    InitializeObjectAttributes(&attributes, &keyPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    if (NT_SUCCESS(ZwOpenKey(&key, KEY_SET_VALUE, &attributes))) {
        RtlInitUnicodeString(&value, ValueName);
        ZwSetValueKey(key, &value, 0, REG_DWORD, &Status, sizeof(ULONG));
        ZwClose(key);
    }
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    BarleyWriteDiag(L"DiagReached", 0);
    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);
    WDF_DRIVER_CONFIG_INIT(&config, BarleyTouchEvtDeviceAdd);
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE);
    BarleyWriteDiag(L"DiagCreateStatus", status);
    return status;
}
