#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>

/*
 * Lenovo TB330XU (Barley) touchscreen hardware contract.
 *
 * The live Lenovo FDT and the stock Android driver identify an HX83102J
 * zero-flash controller on MT6768 SPI0.  ACPI supplies the MMIO resources in
 * this exact order; physical addresses are intentionally not compiled into
 * the Windows driver.
 */

#define BARLEY_TOUCH_FIRMWARE_NAME       L"\\SystemRoot\\System32\\drivers\\Himax_firmware_boe.bin"
#define BARLEY_TOUCH_FIRMWARE_SIZE       261120U
#define BARLEY_TOUCH_MAX_CONTACTS        10U
#define BARLEY_TOUCH_X_MAX               11999U
#define BARLEY_TOUCH_Y_MAX               19199U
#define BARLEY_TOUCH_EVENT_SIZE          56U
#define BARLEY_TOUCH_POLL_PERIOD_MS      8U

#define BARLEY_TOUCH_REPORT_ID           1U
#define BARLEY_TOUCH_FEATURE_REPORT_ID   2U

#define BARLEY_TOUCH_POOL_TAG            'TxHB'

/* ACPI BAR0002 memory-resource order. */
typedef enum _BARLEY_TOUCH_RESOURCE_INDEX {
    BarleyTouchResourceSpi = 0,
    BarleyTouchResourceTopCkgen,
    BarleyTouchResourceInfraCfg,
    BarleyTouchResourceGpio,
    BarleyTouchResourceCount
} BARLEY_TOUCH_RESOURCE_INDEX;

typedef struct _BARLEY_MTK_SPI {
    PUCHAR Registers;
    ULONG Length;
    PUCHAR TopCkgen;
    ULONG TopCkgenLength;
    PUCHAR InfraCfg;
    ULONG InfraCfgLength;
    PUCHAR Gpio;
    ULONG GpioLength;
    ULONG SourceClockHz;
} BARLEY_MTK_SPI, *PBARLEY_MTK_SPI;

#pragma pack(push, 1)
typedef struct _BARLEY_TOUCH_CONTACT_REPORT {
    UCHAR Flags;
    UCHAR ContactId;
    USHORT X;
    USHORT Y;
} BARLEY_TOUCH_CONTACT_REPORT, *PBARLEY_TOUCH_CONTACT_REPORT;

typedef struct _BARLEY_TOUCH_INPUT_REPORT {
    UCHAR ReportId;
    BARLEY_TOUCH_CONTACT_REPORT Contacts[BARLEY_TOUCH_MAX_CONTACTS];
    UCHAR ContactCount;
} BARLEY_TOUCH_INPUT_REPORT, *PBARLEY_TOUCH_INPUT_REPORT;

typedef struct _BARLEY_TOUCH_FEATURE_REPORT {
    UCHAR ReportId;
    UCHAR ContactCountMaximum;
} BARLEY_TOUCH_FEATURE_REPORT, *PBARLEY_TOUCH_FEATURE_REPORT;
#pragma pack(pop)

#define BARLEY_TOUCH_FLAG_TIP_SWITCH 0x01U
#define BARLEY_TOUCH_FLAG_IN_RANGE   0x02U
#define BARLEY_TOUCH_FLAG_CONFIDENCE 0x04U

typedef struct _BARLEY_TOUCH_DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFTIMER PollTimer;
    VHFHANDLE VhfHandle;
    BARLEY_MTK_SPI Spi;

    BOOLEAN HardwareReady;
    BOOLEAN VhfStarted;
    BOOLEAN HaveLastReport;
    ULONG InitializationStage;
    NTSTATUS InitializationStatus;

    PUCHAR Firmware;
    ULONG FirmwareLength;
    PUCHAR ConfigBuffer;
    ULONG ConfigBufferLength;

    UCHAR SpiTxBuffer[1032];
    UCHAR SpiRxBuffer[1032];
    UCHAR EventBuffer[BARLEY_TOUCH_EVENT_SIZE];
    BARLEY_TOUCH_INPUT_REPORT CurrentReport;
    BARLEY_TOUCH_INPUT_REPORT LastReport;
    HID_XFER_PACKET VhfInputPacket;
} BARLEY_TOUCH_DEVICE_CONTEXT, *PBARLEY_TOUCH_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    BARLEY_TOUCH_DEVICE_CONTEXT,
    BarleyTouchGetContext);

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD BarleyTouchEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BarleyTouchEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE BarleyTouchEvtReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY BarleyTouchEvtD0Entry;
EVT_WDF_DEVICE_D0_EXIT BarleyTouchEvtD0Exit;
EVT_WDF_TIMER BarleyTouchEvtPollTimer;
EVT_WDF_OBJECT_CONTEXT_CLEANUP BarleyTouchEvtDeviceCleanup;

EVT_VHF_ASYNC_OPERATION BarleyTouchEvtVhfGetFeature;

NTSTATUS
BarleyMtkSpiInitialize(
    _Inout_ PBARLEY_MTK_SPI Spi
    );

VOID
BarleyMtkSpiResetTouch(
    _Inout_ PBARLEY_MTK_SPI Spi
    );

NTSTATUS
BarleyMtkSpiTransfer(
    _Inout_ PBARLEY_MTK_SPI Spi,
    _In_reads_bytes_(Length) const UCHAR *Transmit,
    _Out_writes_bytes_opt_(Length) UCHAR *Receive,
    _In_ ULONG Length
    );

NTSTATUS
BarleyHimaxInitialize(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    );

NTSTATUS
BarleyHimaxReadTouchReport(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context,
    _Out_ PBARLEY_TOUCH_INPUT_REPORT Report
    );

VOID
BarleyHimaxReleaseFirmware(
    _Inout_ PBARLEY_TOUCH_DEVICE_CONTEXT Context
    );
