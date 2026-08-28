#include "MsdcDxe.h"

#define MSDC_POLL_DELAY_US          100U
#define MSDC_PIO_POLL_DELAY_US      1U
#define MSDC_CONTROL_TIMEOUT_US     100000U
#define MSDC_BUSY_TIMEOUT_US        20000U
#define MSDC_COMMAND_TIMEOUT_US     1000000U
#define MSDC_DATA_TIMEOUT_US        5000000U
#define MSDC_OCR_RETRY_COUNT_EMMC   10U
#define MSDC_OCR_RETRY_COUNT_SD     100U

STATIC MSDC_PRIVATE_DATA *mMsdcHosts[32];
STATIC EFI_EVENT          mMsdcExitBootServicesEvent;

EFI_STATUS
CardReset (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru
  );

STATIC
UINTN
MsdcPollCount (
  IN UINT64 PacketTimeout,
  IN UINT32 DefaultTimeoutUs,
  IN UINT32 PollDelayUs
  )
{
  UINT64 TimeoutUs;

  // EDK2's SD/eMMC bus drivers express this pass-through timeout in
  // microseconds. Convert it to iterations at the caller's polling cadence.
  // A zero packet timeout has no caller-provided bound, so use the bounded
  // operation default.  Otherwise honor SdDxe's size-derived timeout: large
  // multi-block transfers can legitimately require more than five seconds,
  // especially on a removable card running at a conservative bus rate.
  TimeoutUs = (PacketTimeout == 0) ? DefaultTimeoutUs : PacketTimeout;

  return (UINTN)((TimeoutUs + PollDelayUs - 1) / PollDelayUs);
}

MSDC_PRIVATE_DATA gMSDCPrivateDataTemplate = {
  MSDC_PRIVATE_SIGNATURE, // Signature
  NULL, // ControllerHandle
  {
    sizeof (UINT32),
    MsdcPassThru, // PassThru
    MsdcGetNextSlot, // GetNextSlot
    MsdcBuildDevicePath, // BuildDevicePath
    MsdcGetSlotNumber, // GetSlotNumber
    MsdcResetDevice  // ResetDevice
  }, // PassThru
  0, // Index
  0, // MsdcMmioReg
  0, // TopMmioReg
  {0}, // HostData
  {0}  // SdInfo
};

typedef struct {
  VENDOR_DEVICE_PATH Mmc;
  EFI_DEVICE_PATH    End;
} MSDC_DEVICE_PATH;

MSDC_DEVICE_PATH gMSDCDevicePathTemplate = {
  {  // Mmc (VENDOR_DEVICE_PATH)
    {  // Header
      HARDWARE_DEVICE_PATH,      // Type
      HW_VENDOR_DP,              // SubType
      {                          // Length (UINT8 array style)
        (UINT8)(sizeof(VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    // Vendor GUID
    { 0xb615f1f5, 0x5088, 0x43cd, { 0x80, 0x9c, 0xa1, 0x6e, 0x52, 0x48, 0x7d, 0x00 } }
  },
  {  // End (EFI_DEVICE_PATH)
    END_DEVICE_PATH_TYPE,        // Type
    END_ENTIRE_DEVICE_PATH_SUBTYPE,  // SubType
    {                            // Length (UINT8 array style)
      sizeof(EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};

EMMC_DEVICE_PATH gEMMCDevicePathTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_EMMC_DP,
    {
      (UINT8)(sizeof (EMMC_DEVICE_PATH)),
      (UINT8)((sizeof (EMMC_DEVICE_PATH)) >> 8)
    }
  },
  0
};

SD_DEVICE_PATH gSDDevicePathTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_SD_DP,
    {
      (UINT8)(sizeof (SD_DEVICE_PATH)),
      (UINT8)((sizeof (SD_DEVICE_PATH)) >> 8)
    }
  },
  0
};

EFI_STATUS
MsdcPassThru (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  UINT8 Slot,
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet,
  EFI_EVENT Event)
{
  MSDC_PRIVATE_DATA *Private;
  EFI_STATUS         Status;

  if ((This == NULL) || (Packet == NULL) || (Slot != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->SdMmcCmdBlk == NULL) || (Packet->SdMmcStatusBlk == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->OutDataBuffer == NULL) && (Packet->OutTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->InDataBuffer == NULL) && (Packet->InTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MSDC_PRIVATE_FROM_THIS (This);

  Status = MsdcSendCmd (Private, Packet);
  Packet->TransactionStatus = Status;

  // The MSDC host is polling-only, so complete nonblocking requests before
  // returning and signal the caller's completion event.  SdDxe publishes
  // BlockIo2 on top of this protocol; rejecting Event would advertise an
  // asynchronous storage path that can never complete.
  if (Event != NULL) {
    return gBS->SignalEvent (Event);
  }

  return Status;
}

EFI_STATUS
MsdcGetNextSlot (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  UINT8 *Slot)
{
  MSDC_PRIVATE_DATA *Private;

  if (This == NULL || Slot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MSDC_PRIVATE_FROM_THIS (This);

  if (*Slot == 0xFF) {
    *Slot = 0;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
MsdcBuildDevicePath (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  UINT8 Slot,
  EFI_DEVICE_PATH_PROTOCOL **DevicePath)
{
  MSDC_PRIVATE_DATA *Private;
  SD_DEVICE_PATH    *SdNode;
  EMMC_DEVICE_PATH  *EmmcNode;

  if (This == NULL || DevicePath == NULL || Slot != 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MSDC_PRIVATE_FROM_THIS (This);

  if (Private->SdInfo.CardType == EmmcCard) {
    EmmcNode = AllocateCopyPool (sizeof (EMMC_DEVICE_PATH), &gEMMCDevicePathTemplate);
    if (EmmcNode == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)EmmcNode;
  } else if (Private->SdInfo.CardType == SdCard) {
    SdNode = AllocateCopyPool (sizeof (SD_DEVICE_PATH), &gSDDevicePathTemplate);
    if (SdNode == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)SdNode;
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcGetSlotNumber (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  EFI_DEVICE_PATH_PROTOCOL *DevicePath,
  UINT8 *Slot)
{
  MSDC_PRIVATE_DATA *Private;

  if ((This == NULL) || (DevicePath == NULL) || (Slot == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MSDC_PRIVATE_FROM_THIS (This);

  if ((DevicePath->Type != MESSAGING_DEVICE_PATH) ||
      ((DevicePath->SubType != MSG_SD_DP) &&
       (DevicePath->SubType != MSG_EMMC_DP)) ||
      (DevicePathNodeLength (DevicePath) != sizeof (SD_DEVICE_PATH)) ||
      (DevicePathNodeLength (DevicePath) != sizeof (EMMC_DEVICE_PATH)))
  {
    return EFI_UNSUPPORTED;
  }

  *Slot = 0;

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcResetDevice (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  UINT8 Slot)
{
  MSDC_PRIVATE_DATA *Private;

  if ((This == NULL) || (Slot != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MSDC_PRIVATE_FROM_THIS (This);
  if (Private->SdInfo.CardType == UnknownCard) {
    return EFI_NO_MEDIA;
  }

  // All MSDC transactions complete synchronously, so there is no queued I/O
  // to abort here.  Do not electrically reset or re-identify the card: the
  // generic SdDxe BlockIo reset contract only requires outstanding requests
  // to be quiesced, matching the reference host-controller implementation.
  return EFI_SUCCESS;
}

EFI_STATUS
MsdcReset (
  MSDC_PRIVATE_DATA* Private)
{
  UINT32 Reg;

  MsdcSetBits (Private, MSDC_CFG, MSDC_CFG_RST);
  for (UINTN Poll = 0;
       Poll < MsdcPollCount (0, MSDC_CONTROL_TIMEOUT_US, MSDC_POLL_DELAY_US);
       Poll++)
  {
    MsdcRead (Private, MSDC_CFG, &Reg);
    if ((Reg & MSDC_CFG_RST) == 0) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (MSDC_POLL_DELAY_US);
  }

  DEBUG ((DEBUG_ERROR, "MsdcDxe: controller %u reset timed out\n", Private->Index));
  return EFI_TIMEOUT;
}

EFI_STATUS
MsdcClearFifo (
  MSDC_PRIVATE_DATA* Private)
{
  UINT32 Reg;

  MsdcSetBits (Private, MSDC_FIFOCS, MSDC_FIFOCS_CLR);
  for (UINTN Poll = 0;
       Poll < MsdcPollCount (0, MSDC_CONTROL_TIMEOUT_US, MSDC_POLL_DELAY_US);
       Poll++)
  {
    MsdcRead (Private, MSDC_FIFOCS, &Reg);
    if ((Reg & MSDC_FIFOCS_CLR) == 0) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (MSDC_POLL_DELAY_US);
  }

  DEBUG ((DEBUG_ERROR, "MsdcDxe: controller %u FIFO clear timed out\n", Private->Index));
  return EFI_TIMEOUT;
}

VOID
MsdcClearInterrupts (
  MSDC_PRIVATE_DATA* Private)
{
  UINT32 Reg;
  MsdcRead (Private, MSDC_INT, &Reg);
  MsdcWrite (Private, MSDC_INT, Reg);
}

EFI_STATUS
MsdcSetTimeout (
  MSDC_PRIVATE_DATA* Private)
{
  UINT32 CfgReg, Timeout, ClkNs;

  if (Private->HostData.Sclk == 0) {
    return EFI_NOT_READY;
  }

  ClkNs = 1000000000 / Private->HostData.Sclk;
  Timeout = (Private->HostData.TimeoutNs + ClkNs - 1) / ClkNs + Private->HostData.TimeoutClks;
  Timeout = (Timeout + (1 << SCLK_CYCLES_SHIFT) - 1) >> SCLK_CYCLES_SHIFT;
  Timeout = Timeout > 1 ? Timeout - 1 : 0;
  Timeout = Timeout > 255 ? 255 : Timeout;

  MsdcRead (Private, SDC_CFG, &CfgReg);
  // Clear BIT24:BIT31
  CfgReg &= ~(0xff << 24);

  CfgReg |= Timeout << 24;
  MsdcWrite (Private, SDC_CFG, CfgReg);
  return EFI_SUCCESS;
}

VOID
MsdcSetBusWidth (
  MSDC_PRIVATE_DATA* Private,
  UINT32 Width)
{
  UINT32 CfgReg, BusWidth;

  switch (Width) {
    case 1:
      BusWidth = MSDC_BUS_WIDTH_1;
      break;
    case 4:
      BusWidth = MSDC_BUS_WIDTH_4;
      break;
    case 8:
      BusWidth = MSDC_BUS_WIDTH_8;
      break;
    default:
      BusWidth = MSDC_BUS_WIDTH_1;
      break;
  }

  MsdcRead (Private, SDC_CFG, &CfgReg);
  // Clear BIT16:BIT17
  CfgReg &= ~(3 << SDC_CFG_BUS_WIDTH_SHIFT);

  CfgReg |= BusWidth << SDC_CFG_BUS_WIDTH_SHIFT;
  MsdcWrite (Private, SDC_CFG, CfgReg);
}

EFI_STATUS
MsdcSetMclk (
  MSDC_PRIVATE_DATA* Private,
  UINT32 Hz)
{
  EFI_STATUS Status;
  UINTN SourceClock;
  UINT32 CfgReg;
  UINT32 Div;
  UINT32 Mode;

  if (Hz == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetSourceClockRate (Private->Index, &SourceClock);
  if (EFI_ERROR (Status) || (SourceClock == 0)) {
    return EFI_ERROR (Status) ? Status : EFI_NOT_READY;
  }

  if (Hz >= SourceClock) {
    // Ignore divisor
    Div = 0;
    Mode = MSDC_MCLK_NO_DIV;
    Private->HostData.Sclk = SourceClock;
  } else {
    // Divisor mode
    Mode = MSDC_MCLK_DIV;
    if (Hz >= (SourceClock >> 1)) {
      Div = 0; /* Will divide source clock 1/2 */
      Private->HostData.Sclk = SourceClock >> 1;
    } else {
      Div = (SourceClock + ((Hz << 2) - 1)) / (Hz << 2);
      Private->HostData.Sclk = (SourceClock >> 2) / Div;
    }
  }

  DEBUG ((DEBUG_ERROR, "Hz: %d, Mode: %d, Div: %d, Sclk: %d\n", Hz, Mode, Div, Private->HostData.Sclk));

  MsdcRead (Private, MSDC_CFG, &CfgReg);
  // Clear CCKMD (BIT20:21)
  CfgReg &= ~(3 << 20);
  // Clear CCKDIV (BIT8:19)
  CfgReg &= ~(0xfff << 8);
  // Clear hs400 div
  CfgReg &= ~(MSDC_CFG_HS400CKMD);
  // The register snapshot was taken before the clock was gated below.  Do not
  // accidentally restore CCKPD while programming the new divider.
  CfgReg &= ~MSDC_CFG_CCKPD;
  // Set values
  CfgReg |= (Mode << 20);
  CfgReg |= (Div << 8);

  // Stop only the controller's internal clock while changing its divider.
  // LK may still have consumers on the shared MediaTek parent clock tree.
  MsdcClrBits (Private, MSDC_CFG, MSDC_CFG_CCKPD);
  Status = SourceClockControl (Private->Index, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MsdcWrite (Private, MSDC_CFG, CfgReg);
  MsdcSetBits (Private, MSDC_CFG, MSDC_CFG_CCKPD);

  for (UINTN Poll = 0;
       Poll < MsdcPollCount (0, MSDC_CONTROL_TIMEOUT_US, MSDC_POLL_DELAY_US);
       Poll++)
  {
    MsdcRead (Private, MSDC_CFG, &CfgReg);
    if ((CfgReg & MSDC_CFG_CCKSB) != 0) {
      return MsdcSetTimeout (Private);
    }

    MicroSecondDelay (MSDC_POLL_DELAY_US);
  }

  DEBUG ((DEBUG_ERROR, "MsdcDxe: controller %u clock did not stabilize\n", Private->Index));
  MsdcClrBits (Private, MSDC_CFG, MSDC_CFG_CCKPD);
  return EFI_TIMEOUT;
}

#define MSDC_INT_CMDERR (MSDC_INT_CMDTMO | MSDC_INT_CMDCRCERR)
#define MSDC_INT_ACMDERR (MSDC_INT_ACMDTMO | MSDC_INT_ACMDCRCERR)
#define MSDC_INT_DATERR (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR)
#define MSDC_INT_CMDSTS (MSDC_INT_CMDRDY | MSDC_INT_CMDERR)
#define MSDC_INT_AUTOSTS (MSDC_INT_ACMDRDY | MSDC_INT_ACMDERR)
#define MSDC_INT_DATSTS (MSDC_INT_XFER_COMPL | MSDC_INT_DATERR | MSDC_INT_AUTOSTS)

EFI_STATUS
MsdcWaitReady (
  MSDC_PRIVATE_DATA* Private,
  BOOLEAN WaitDataBusy,
  UINT64 PacketTimeout)
{
  UINT32 Reg;
  UINT32 BusyMask;

  // Linux's MediaTek host driver always waits for CMDBUSY to clear and also
  // waits for SDCBUSY for R1B and data commands.  The two bits are not
  // interchangeable.
  BusyMask = SDC_STS_CMDBUSY;
  if (WaitDataBusy) {
    BusyMask |= SDC_STS_BUSY;
  }

  for (UINTN Poll = 0;
       Poll < MsdcPollCount (PacketTimeout, MSDC_BUSY_TIMEOUT_US, MSDC_POLL_DELAY_US);
       Poll++)
  {
    MsdcRead (Private, SDC_STS, &Reg);
    if ((Reg & BusyMask) == 0) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (MSDC_POLL_DELAY_US);
  }

  DEBUG ((
    DEBUG_ERROR,
    "MsdcDxe: controller %u remained busy (mask 0x%x)\n",
    Private->Index,
    BusyMask
    ));
  return EFI_TIMEOUT;
}

EFI_STATUS
MsdcIntTrackError (
  UINT32 IntStatus)
{
  if (IntStatus & MSDC_INT_CMDTMO) {
    return EFI_TIMEOUT;
  }

  if (IntStatus & MSDC_INT_CMDCRCERR) {
    return EFI_CRC_ERROR;
  }

  if (IntStatus & MSDC_INT_ACMDTMO) {
    return EFI_TIMEOUT;
  }

  if (IntStatus & MSDC_INT_ACMDCRCERR) {
    return EFI_CRC_ERROR;
  }

  if (IntStatus & MSDC_INT_DATTMO) {
    return EFI_TIMEOUT;
  }

  if (IntStatus & MSDC_INT_DATCRCERR) {
    return EFI_CRC_ERROR;
  }

  return EFI_DEVICE_ERROR;
}

EFI_STATUS
MsdcPollInterrupts (
  MSDC_PRIVATE_DATA* Private,
  UINT32 ExpectedInterrupts,
  UINT32 SuccessInterrupts,
  UINT64 PacketTimeout)
{
  UINT32 IntStatus;
  UINT32 ObservedInterrupts;

  for (UINTN Poll = 0;
       Poll < MsdcPollCount (PacketTimeout, MSDC_COMMAND_TIMEOUT_US, MSDC_POLL_DELAY_US);
       Poll++)
  {
    MsdcRead (Private, MSDC_INT, &IntStatus);
    ObservedInterrupts = IntStatus & ExpectedInterrupts;
    if (ObservedInterrupts != 0) {
      // MSDC_INT is write-one-to-clear.  Clear every status observed for this
      // operation so a stale success/error cannot poison the next command.
      MsdcWrite (Private, MSDC_INT, ObservedInterrupts);

      // Errors win when hardware reports ready and an error simultaneously.
      if ((ObservedInterrupts &
           (MSDC_INT_CMDERR | MSDC_INT_ACMDERR | MSDC_INT_DATERR)) != 0)
      {
        return MsdcIntTrackError (ObservedInterrupts);
      }

      if ((ObservedInterrupts & SuccessInterrupts) == SuccessInterrupts) {
        return EFI_SUCCESS;
      }
    }

    MicroSecondDelay (MSDC_POLL_DELAY_US);
  }

  DEBUG ((DEBUG_ERROR, "MsdcDxe: controller %u command interrupt timed out\n", Private->Index));
  return EFI_TIMEOUT;
}

VOID
MsdcFifoRxBytes (
  MSDC_PRIVATE_DATA* Private,
  UINT32 *RxBytes)
{
  UINT32 FifoCs;

  MsdcRead (Private, MSDC_FIFOCS, &FifoCs);
  *RxBytes = FifoCs & 0xff;
}

VOID
MsdcFifoTxBytes (
  MSDC_PRIVATE_DATA* Private,
  UINT32 *TxBytes)
{
  UINT32 FifoCs;

  MsdcRead (Private, MSDC_FIFOCS, &FifoCs);
  *TxBytes = (FifoCs >> 16) & 0xff;
}

VOID
MsdcFifoRead (
  MSDC_PRIVATE_DATA* Private,
  UINT8 *ByteBuffer,
  UINT32 BufferLength)
{
  UINT32 RemainSize = BufferLength;

  // Pointer align
  while ((UINTN)ByteBuffer % 4 != 0 && RemainSize > 0) {
    *ByteBuffer = MmioRead8 (Private->MsdcMmioReg + MSDC_RXDATA);
    ByteBuffer++;
    RemainSize--;
  }

  while (RemainSize >= 4) {
    *(UINT32 *)ByteBuffer = MmioRead32 (Private->MsdcMmioReg + MSDC_RXDATA);
    ByteBuffer += 4;
    RemainSize -= 4;
  }

  while (RemainSize) {
    *ByteBuffer = MmioRead8 (Private->MsdcMmioReg + MSDC_RXDATA);
    ByteBuffer++;
    RemainSize--;
  }
}

VOID
MsdcFifoWrite (
  MSDC_PRIVATE_DATA* Private,
  UINT8 *ByteBuffer,
  UINT32 BufferLength)
{
  UINT32 RemainSize = BufferLength;

  // Pointer align
  while ((UINTN)ByteBuffer % 4 != 0 && RemainSize > 0) {
    MmioWrite8 (Private->MsdcMmioReg + MSDC_TXDATA, *ByteBuffer);
    ByteBuffer++;
    RemainSize--;
  }

  while (RemainSize >= 4) {
    MmioWrite32 (Private->MsdcMmioReg + MSDC_TXDATA, *(UINT32 *)ByteBuffer);
    ByteBuffer += 4;
    RemainSize -= 4;
  }

  while (RemainSize) {
    MmioWrite8 (Private->MsdcMmioReg + MSDC_TXDATA, *ByteBuffer);
    ByteBuffer++;
    RemainSize--;
  }
}

EFI_STATUS
MsdcPioRead (
  MSDC_PRIVATE_DATA* Private,
  VOID  *Buffer,
  UINT32 BufferLength,
  UINT64 PacketTimeout)
{
  BOOLEAN TransferComplete;
  UINT32 IntStatus, ObservedInterrupts, ChunkSize, RemainSize, RxBytes;
  UINT8 *ByteBuffer = (UINT8 *)Buffer;
  UINTN PollCount;

  RemainSize = BufferLength;
  PollCount = MsdcPollCount (PacketTimeout, MSDC_DATA_TIMEOUT_US, MSDC_PIO_POLL_DELAY_US);
  TransferComplete = FALSE;

  MsdcClrBits (Private, MSDC_INTEN, MSDC_INT_DATSTS);

  for (UINTN Poll = 0; Poll < PollCount; Poll++) {
    MsdcRead (Private, MSDC_INT, &IntStatus);
    ObservedInterrupts = IntStatus & MSDC_INT_DATSTS;

    // Service the FIFO at data-path cadence.  Waiting the control-path 100 us
    // here can overflow or stall the 128-byte FIFO at normal SD/MMC clocks.
    // Read full FIFO chunks, matching the original MediaTek PIO algorithm;
    // only the final transfer chunk may be smaller than MSDC_FIFO_SIZE.
    MsdcFifoRxBytes (Private, &RxBytes);
    ChunkSize = RemainSize > MSDC_FIFO_SIZE ? MSDC_FIFO_SIZE : RemainSize;
    if ((ChunkSize != 0) && (RxBytes >= ChunkSize)) {
      MsdcFifoRead (Private, ByteBuffer, ChunkSize);
      ByteBuffer += ChunkSize;
      RemainSize -= ChunkSize;
    }

    if (ObservedInterrupts != 0) {
      MsdcWrite (Private, MSDC_INT, ObservedInterrupts);
    }

    if ((ObservedInterrupts & (MSDC_INT_DATERR | MSDC_INT_ACMDERR)) != 0) {
      return MsdcIntTrackError (ObservedInterrupts);
    }

    if ((ObservedInterrupts & MSDC_INT_XFER_COMPL) != 0) {
      TransferComplete = TRUE;
    }

    if (TransferComplete && (RemainSize == 0)) {
      break;
    }

    MicroSecondDelay (MSDC_PIO_POLL_DELAY_US);
  }

  if (!TransferComplete || (RemainSize != 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "MsdcDxe: controller %u PIO read timed out: remaining=%u int=0x%08x "
      "fifo=%u complete=%u\n",
      Private->Index,
      RemainSize,
      IntStatus,
      RxBytes,
      TransferComplete
      ));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcPioWrite (
  MSDC_PRIVATE_DATA* Private,
  VOID  *Buffer,
  UINT32 BufferLength,
  UINT64 PacketTimeout)
{
  BOOLEAN TransferComplete;
  UINT32 IntStatus, ObservedInterrupts, ChunkSize, RemainSize, TxBytes;
  UINT8 *ByteBuffer = (UINT8 *)Buffer;
  UINTN PollCount;

  RemainSize = BufferLength;
  PollCount = MsdcPollCount (PacketTimeout, MSDC_DATA_TIMEOUT_US, MSDC_PIO_POLL_DELAY_US);
  TransferComplete = FALSE;

  MsdcClrBits (Private, MSDC_INTEN, MSDC_INT_DATSTS);

  for (UINTN Poll = 0; Poll < PollCount; Poll++) {
    MsdcRead (Private, MSDC_INT, &IntStatus);
    ObservedInterrupts = IntStatus & MSDC_INT_DATSTS;

    if (ObservedInterrupts != 0) {
      MsdcWrite (Private, MSDC_INT, ObservedInterrupts);
    }

    if ((ObservedInterrupts & (MSDC_INT_DATERR | MSDC_INT_ACMDERR)) != 0) {
      return MsdcIntTrackError (ObservedInterrupts);
    }

    if ((ObservedInterrupts & MSDC_INT_XFER_COMPL) != 0) {
      if (RemainSize) {
        DEBUG ((DEBUG_ERROR, "MsdcDxe: Data not fully wrote!\n"));
        return EFI_ABORTED;
      }

      TransferComplete = TRUE;
      break;
    }

    ChunkSize = RemainSize > MSDC_FIFO_SIZE ? MSDC_FIFO_SIZE : RemainSize;

    MsdcFifoTxBytes (Private, &TxBytes);
    if (MSDC_FIFO_SIZE - TxBytes >= ChunkSize) {
      MsdcFifoWrite (Private, ByteBuffer, ChunkSize);
      ByteBuffer += ChunkSize;
      RemainSize -= ChunkSize;
    }

    MicroSecondDelay (MSDC_PIO_POLL_DELAY_US);
  }

  if (!TransferComplete) {
    DEBUG ((
      DEBUG_ERROR,
      "MsdcDxe: controller %u PIO write timed out: remaining=%u int=0x%08x "
      "fifo=%u\n",
      Private->Index,
      RemainSize,
      IntStatus,
      TxBytes
      ));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcSendCmd (
  MSDC_PRIVATE_DATA* Private,
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet)
{
  BOOLEAN IsDataTransfer, IsRead, WaitDataBusy;
  EFI_STATUS Status;
  UINT32 RawCmd, RspType, BlkLen, BlkSize, TransferLength;
  UINT32 RxBytes, TxBytes;
  EFI_SD_MMC_COMMAND_BLOCK *CommandBlk = Packet->SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK  *SdMmcStatusBlk = Packet->SdMmcStatusBlk;

  RawCmd = CommandBlk->CommandIndex;
  IsDataTransfer = FALSE;
  IsRead = TRUE;
  BlkLen = 0;
  BlkSize = 0;
  TransferLength = 0;

  if (CommandBlk->CommandType != SdMmcCommandTypeBc) {
    switch (CommandBlk->ResponseType) {
      case SdMmcResponseTypeR1:
      case SdMmcResponseTypeR5:
      case SdMmcResponseTypeR6:
      case SdMmcResponseTypeR7:
        RspType = 1;
        break;
      case SdMmcResponseTypeR2:
        RspType = 2;
        break;
      case SdMmcResponseTypeR3:
        RspType = 3;
        break;
      case SdMmcResponseTypeR4:
        RspType = 4;
        break;
      case SdMmcResponseTypeR1b:
        RspType = 7;
        break;
      case SdMmcResponseTypeR5b:
      default:
        return EFI_INVALID_PARAMETER;
    }
    RawCmd |= RspType << SDC_CMD_RSP_TYPE_SHIFT;
  }

  if ((Packet->InTransferLength != 0) && (Packet->OutTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Packet->InTransferLength != 0) {
    IsDataTransfer = TRUE;
    IsRead = TRUE;
    TransferLength = Packet->InTransferLength;
  } else if (Packet->OutTransferLength != 0) {
    IsDataTransfer = TRUE;
    IsRead = FALSE;
    TransferLength = Packet->OutTransferLength;
  }

  if (IsDataTransfer !=
      (CommandBlk->CommandType == SdMmcCommandTypeAdtc))
  {
    return EFI_INVALID_PARAMETER;
  }

  if (((CommandBlk->CommandIndex == SD_READ_SINGLE_BLOCK) ||
       (CommandBlk->CommandIndex == SD_WRITE_SINGLE_BLOCK)) &&
      (TransferLength != BLOCK_SIZE))
  {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (CommandBlk->CommandIndex == SD_STOP_TRANSMISSION) {
    // stop command
    RawCmd |= SDC_CMD_STOP_CMD;
  }

  if (IsDataTransfer) {
    if ((CommandBlk->CommandIndex == SD_READ_MULTIPLE_BLOCK) ||
        (CommandBlk->CommandIndex == SD_WRITE_MULTIPLE_BLOCK))
    {
      if ((TransferLength % BLOCK_SIZE) != 0) {
        return EFI_BAD_BUFFER_SIZE;
      }

      BlkSize = BLOCK_SIZE;
      BlkLen = TransferLength / BLOCK_SIZE;
      RawCmd |= SDC_CMD_MULTIPLE_BLK;
      if (Private->SdInfo.CardType == SdCard) {
        RawCmd |= SDC_CMD_AUTO12;
      }
    } else {
      // ADTC commands such as SD CMD6/CMD51 use 64/8-byte data blocks,
      // while ordinary single-block I/O and eMMC EXT_CSD use 512 bytes.
      BlkSize = TransferLength;
      BlkLen = 1;
      RawCmd |= SDC_CMD_SINGLE_BLK;
    }

    if ((BlkSize == 0) || (BlkSize > 0xFFF)) {
      return EFI_BAD_BUFFER_SIZE;
    }

    if (!IsRead) {
      RawCmd |= SDC_CMD_RW;
    }

    RawCmd |= BlkSize << SDC_CMD_BLK_SIZE_SHIFT;
    MsdcWrite (Private, SDC_BLK_NUM, BlkLen);
  }

  WaitDataBusy = IsDataTransfer ||
                 (CommandBlk->ResponseType == SdMmcResponseTypeR1b) ||
                 (CommandBlk->ResponseType == SdMmcResponseTypeR5b);
  Status = MsdcWaitReady (Private, WaitDataBusy, Packet->Timeout);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Disable interrupts because we use polling way
  MsdcClrBits (Private, MSDC_INTEN, MSDC_INT_CMDSTS | MSDC_INT_DATSTS);

  // A non-empty FIFO means the preceding transaction did not retire cleanly.
  // Recover before issuing a new command, matching the upstream Linux host
  // driver's reset/FIFO-clear behavior.
  MsdcFifoRxBytes (Private, &RxBytes);
  MsdcFifoTxBytes (Private, &TxBytes);
  if ((RxBytes != 0) || (TxBytes != 0)) {
    Status = MsdcReset (Private);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = MsdcClearFifo (Private);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  // MSDC_INT is W1C.  Remove every stale status immediately before SDC_CMD.
  MsdcClearInterrupts (Private);

  MsdcWrite (Private, SDC_ARG, CommandBlk->CommandArgument);
  MsdcWrite (Private, SDC_CMD, RawCmd);

  Status = MsdcPollInterrupts (Private, MSDC_INT_CMDSTS, MSDC_INT_CMDRDY, Packet->Timeout);
  if (EFI_ERROR(Status)) {
    MsdcReset (Private);
    MsdcClearFifo (Private);
    MsdcClearInterrupts (Private);
    return Status;
  }

  if (CommandBlk->CommandType != SdMmcCommandTypeBc) {
    if (CommandBlk->ResponseType == SdMmcResponseTypeR2) {
      // Native MediaTek MSDC ordering is RESP3, RESP2, RESP1, RESP0.  This
      // controller is not SDHCI and must not use SDHCI's one-byte R2 shift.
      MsdcRead (Private, SDC_RESP3, &SdMmcStatusBlk->Resp0);
      MsdcRead (Private, SDC_RESP2, &SdMmcStatusBlk->Resp1);
      MsdcRead (Private, SDC_RESP1, &SdMmcStatusBlk->Resp2);
      MsdcRead (Private, SDC_RESP0, &SdMmcStatusBlk->Resp3);
    } else {
      MsdcRead (Private, SDC_RESP0, &SdMmcStatusBlk->Resp0);
    }
  }

  if (IsDataTransfer) {
    if (IsRead) {
      Status = MsdcPioRead (Private, Packet->InDataBuffer, Packet->InTransferLength, Packet->Timeout);
      if (EFI_ERROR(Status)) {
        MsdcReset (Private);
        MsdcClearFifo (Private);
        MsdcClearInterrupts (Private);
        return Status;
      }
    } else {
      Status = MsdcPioWrite (Private, Packet->OutDataBuffer, Packet->OutTransferLength, Packet->Timeout);
      if (EFI_ERROR(Status)) {
        MsdcReset (Private);
        MsdcClearFifo (Private);
        MsdcClearInterrupts (Private);
        return Status;
      }
    }
  }

  return Status;
}

STATIC
VOID
EFIAPI
MsdcExitBootServices (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS Status;
  UINTN Index;
  MSDC_PRIVATE_DATA *Private;

  (VOID)Event;
  (VOID)Context;

  for (Index = 0; Index < ARRAY_SIZE (mMsdcHosts); Index++) {
    Private = mMsdcHosts[Index];
    if (Private == NULL) {
      continue;
    }

    /*
     * UEFI may use 4-bit SD or 8-bit eMMC for fast boot I/O.  CMD0 has no
     * response or data phase and returns the card to its protocol-default
     * 1-bit identification state before the host width is narrowed.  Windows
     * SDPORT can then begin a fresh 400-kHz identification sequence without
     * inheriting a card/host width mismatch.  Power, parent clocks and pinmux
     * deliberately remain enabled for the OS handoff.
     */
    Status = CardReset (&Private->PassThru);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "MsdcDxe: host %u CMD0 handoff failed: %r\n",
              Private->Index, Status));
    }

    MsdcSetBusWidth (Private, 1);
    Status = MsdcReset (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "MsdcDxe: host %u reset handoff failed: %r\n",
              Private->Index, Status));
    }
    (VOID)MsdcClearFifo (Private);
    MsdcClearInterrupts (Private);
    Status = MsdcSetMclk (Private, 400000);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "MsdcDxe: host %u 400-kHz handoff failed: %r\n",
              Private->Index, Status));
    }
  }
}

EFI_STATUS MsdcInit (
  MSDC_PRIVATE_DATA *Private)
{
  EFI_STATUS Status;

  Status = InitGpio (Private->Index);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ClockControl (Private->Index, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Configure to SD/MMC mode, Clock free running, PIO mode
  MsdcSetBits (Private, MSDC_CFG, MSDC_CFG_MODE | MSDC_CFG_CCKPD | MSDC_CFG_PIO);

  // SW Reset
  Status = MsdcReset (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Clear FIFO
  Status = MsdcClearFifo (Private);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Mask all interrupts
  MsdcClearInterrupts (Private);

  // Enable SDIO mode, otherwise cmd5 won't work
  // This bit disables R4 response CRC check for SDIO card
  MsdcSetBits (Private, SDC_CFG, SDC_CFG_SDIO);
  // Disable detecting SDIO interrupts
  MsdcClrBits (Private, SDC_CFG, SDC_CFG_SDIOIDE);

  if (gPlatformInfo.UseTop) {
    MsdcTopWrite (Private, TOP_CTRL, 0);
    MsdcTopWrite (Private, TOP_CMD, 0);
  } else {
    MsdcWrite (Private, gPlatformInfo.MsdcPadTuneReg, 0);
  }

  MsdcWrite (Private, MSDC_IOCON, 0);

  // Quirks
  MsdcWrite (Private, MSDC_PATCH_BIT0, 0x403c0446);
  MsdcWrite (Private, MSDC_PATCH_BIT1, 0xffff4089);

  MsdcSetBits (Private, EMMC50_CFG0, EMMC50_CFG0_CRCSTSSEL);

  if (gPlatformInfo.BusyCheck) {
    MsdcClrBits (Private, MSDC_PATCH_BIT1, MSDC_PB1_BUSYCHECKSEL);
  }

  if (gPlatformInfo.StopClkFix) {
    MsdcSetBits (Private, MSDC_PATCH_BIT1, BIT8 | BIT9);
    MsdcClrBits (Private, SDC_FIFO_CFG, SDC_FIFO_CFG_WRVALIDSEL);
    MsdcClrBits (Private, SDC_FIFO_CFG, SDC_FIFO_CFG_RDVALIDSEL);
  }

  if (gPlatformInfo.AsyncFifo)
  {
    MsdcClrSetBits(Private, MSDC_PATCH_BIT2, MSDC_PB2_RESPWAIT, 3 << MSDC_PB2_RESPWAIT_SHIFT);

    MsdcClrBits (Private, MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
    MsdcSetBits (Private, MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);

    if (gPlatformInfo.EnhanceRx)
    {
      if (gPlatformInfo.UseTop) {
        MsdcTopSetBits (Private, TOP_CTRL, SDC_RX_ENH_EN);
      } else {
        MsdcSetBits (Private, SDC_ADV_CFG0, SDC_RX_ENHANCE_EN);
      }
    } else {
      MsdcClrSetBits(Private, MSDC_PATCH_BIT2, MSDC_PB2_RESPSTSENSEL | MSDC_PB2_CRCSTSENSEL, (2 << 16) | (2 << 29));
    }
  }

  // Data tune
  if (gPlatformInfo.UseTop) {
    MsdcTopSetBits (Private, TOP_CTRL, PAD_DAT_RD_RXDLY_SEL);
    MsdcTopClrBits (Private, TOP_CTRL, DATA_K_VALUE_SEL);
    MsdcTopSetBits (Private, TOP_CMD, PAD_CMD_RD_RXDLY_SEL);
    if (gPlatformInfo.TuningStep[Private->Index] > 32)
    {
      MsdcTopSetBits (Private, TOP_CTRL, PAD_DAT_RD_RXDLY2_SEL);
      MsdcTopSetBits (Private, TOP_CMD, PAD_CMD_RD_RXDLY2_SEL);
    }
  } else {
    MsdcSetBits (Private, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_RD_SEL | MSDC_PAD_TUNE_CMD_SEL);
  }

  // Set default data timeout
  Private->HostData.TimeoutNs = 100000000;
  Private->HostData.TimeoutClks = 3 * (1 << SCLK_CYCLES_SHIFT);

  // Set default bus width
  MsdcSetBusWidth (Private, 1);

  return EFI_SUCCESS;
}

EFI_STATUS
CardReset (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_GO_IDLE_STATE;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBc;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  return Status;
}

EFI_STATUS
SdSendIfCond (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_SEND_IF_COND;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR7;
  SdMmcCmdBlk.CommandArgument = 0x1AA;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  return Status;
}

EFI_STATUS
SdCardSendOpCond (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT16                         Rca,
  UINT32                         VoltageWindow,
  BOOLEAN                        S18R,
  BOOLEAN                        Xpc,
  BOOLEAN                        Hcs,
  UINT32                        *Ocr)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT32 Switch;
  UINT32 MaxPower;
  UINT32 HostCapacity;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_APP_CMD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdMmcCmdBlk.CommandIndex = SD_SEND_OP_COND;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR3;

  Switch       = S18R ? BIT24 : 0;
  MaxPower     = Xpc ? BIT28 : 0;
  HostCapacity = Hcs ? BIT30 : 0;

  SdMmcCmdBlk.CommandArgument = (VoltageWindow & 0xFFFFFF) | Switch | \
                                MaxPower | HostCapacity;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  if (!EFI_ERROR (Status)) {
    *Ocr = SdMmcStatusBlk.Resp0;
  }

  return Status;
}

EFI_STATUS
EMMCSendOpCond (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT32                         VoltageWindow,
  BOOLEAN                        Hcs,
  UINT32                        *Ocr)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT32 HostCapacity;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = EMMC_SEND_OP_COND;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR3;

  HostCapacity = Hcs ? BIT30 : 0;

  SdMmcCmdBlk.CommandArgument = (VoltageWindow & 0xFFFFFF) | HostCapacity;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  if (!EFI_ERROR (Status)) {
    *Ocr = SdMmcStatusBlk.Resp0;
  }

  return Status;
}

EFI_STATUS
CardAllSendCid (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_ALL_SEND_CID;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR2;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  return Status;
}

EFI_STATUS
SdCardSetRca (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT16                        *Rca)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_SET_RELATIVE_ADDR;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR6;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    *Rca = (UINT16)(SdMmcStatusBlk.Resp0 >> 16);
  }

  return Status;
}

EFI_STATUS
EMMCSetRca (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT16                         Rca)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = EMMC_SET_RELATIVE_ADDR;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  return Status;
}

EFI_STATUS
CardSelect (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT16                         Rca)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_SELECT_DESELECT_CARD;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAc;
  if (Rca != 0) {
    SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1b;
  }

  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  return Status;
}

EFI_STATUS
CardSendStatus (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT16                         Rca,
  UINT32                        *DevStatus)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_SEND_STATUS;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    *DevStatus = SdMmcStatusBlk.Resp0;
  }

  return Status;
}

EFI_STATUS
SdCardSwitch (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT8                          AccessMode,
  UINT8                          CommandSystem,
  UINT8                          DriveStrength,
  UINT8                          PowerLimit,
  BOOLEAN                        Mode,
  UINT8                         *SwitchResp)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT32 ModeValue;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_SWITCH_FUNC;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1;

  ModeValue                   = Mode ? BIT31 : 0;
  SdMmcCmdBlk.CommandArgument = (AccessMode & 0xF) |            \
                                ((PowerLimit & 0xF) << 4) |     \
                                ((DriveStrength & 0xF) << 8) |  \
                                ((DriveStrength & 0xF) << 12) | \
                                ModeValue;

  Packet.InDataBuffer     = SwitchResp;
  Packet.InTransferLength = 64;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  return Status;
}

EFI_STATUS
SdCardSetBusWidth (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT16                         Rca,
  UINT8                          BusWidth)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT32 Argument;

  if (BusWidth == 1) {
    Argument = SD_BUS_WIDTH_1;
  } else if (BusWidth == 4) {
    Argument = SD_BUS_WIDTH_4;
  } else {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_APP_CMD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdMmcCmdBlk.CommandIndex    = SD_SET_BUS_WIDTH;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = Argument;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

EFI_STATUS
EMMCSwitch (
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  UINT8 Access,
  UINT8 Index,
  UINT8 Value,
  UINT8 CmdSet)
{
  EFI_SD_MMC_COMMAND_BLOCK SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = EMMC_SWITCH;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1b;

  SdMmcCmdBlk.CommandArgument = (Access << 24) |
                                (Index << 16) |
                                (Value << 8) |
                                CmdSet;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);

  return Status;
}

EFI_STATUS
CardSetBusMode (
  MSDC_PRIVATE_DATA *Private,
  UINT16             Rca)
{
  EFI_STATUS Status;
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru;
  UINT8 SwitchResp[64];
  UINT32 DevStatus;
  UINT8 BusWidth;

  PassThru = &Private->PassThru;

  Status = CardSelect(PassThru, Rca);
  if (EFI_ERROR(Status))
  {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardSelect failed with status %r\n", Status));
    return Status;
  }

  if (Private->SdInfo.CardType == SdCard) {
    BusWidth = FixedPcdGet8 (PcdMsdcRemovableBusWidth);
    if ((BusWidth != 1) && (BusWidth != 4)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: invalid removable bus width %u\n", BusWidth));
      return EFI_INVALID_PARAMETER;
    }

    if (BusWidth == 4) {
      // The card must accept ACMD6 before the controller begins sampling the
      // additional data lines.  Reversing this order loses the response path.
      Status = SdCardSetBusWidth (PassThru, Rca, BusWidth);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "MsdcDxe: SD ACMD6 failed with status %r; retaining 1-bit mode\n", Status));
        BusWidth = 1;
        MsdcSetBusWidth (Private, BusWidth);
      } else {
        MsdcSetBusWidth (Private, BusWidth);
      }
    } else {
      MsdcSetBusWidth (Private, BusWidth);
    }

    Status = SdCardSwitch(PassThru, 1, 0xF, 0xF, 0xF, TRUE, SwitchResp);
    if (EFI_ERROR(Status))
    {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: SdCardSwitch failed with status %r\n", Status));
      return Status;
    }
  } else {
    BusWidth = FixedPcdGet8 (PcdMsdcEmmcBusWidth);
    if ((BusWidth != 1) && (BusWidth != 4) && (BusWidth != 8)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: invalid eMMC bus width %u\n", BusWidth));
      return EFI_INVALID_PARAMETER;
    }

    if (BusWidth != 1) {
      Status = EMMCSwitch (
                 PassThru,
                 3,
                 EMMC_EXT_CSD_BUS_WIDTH,
                 BusWidth == 8 ? EMMC_BUS_WIDTH_8 : EMMC_BUS_WIDTH_4,
                 0
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "MsdcDxe: eMMC bus-width switch failed with status %r; retaining 1-bit mode\n", Status));
        BusWidth = 1;
        MsdcSetBusWidth (Private, BusWidth);
      } else {
        Status = CardSendStatus (PassThru, Rca, &DevStatus);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "MsdcDxe: CardSendStatus failed with status %r\n", Status));
          return Status;
        }

        if ((DevStatus & BIT7) != 0) {
          DEBUG ((DEBUG_WARN, "MsdcDxe: eMMC rejected bus width %u, status 0x%x; retaining 1-bit mode\n", BusWidth, DevStatus));
          BusWidth = 1;
          MsdcSetBusWidth (Private, BusWidth);
        } else {
          MsdcSetBusWidth (Private, BusWidth);
        }
      }
    } else {
      MsdcSetBusWidth (Private, BusWidth);
    }

    Status = EMMCSwitch (
               PassThru,
               3,
               EMMC_EXT_CSD_HS_TIMING,
               EMMC_HS_TIMING_HIGH_SPEED,
               0
               );
    if (EFI_ERROR(Status))
    {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: EMMCSwitch failed with status %r\n", Status));
      return Status;
    }
  }

  Status = CardSendStatus(PassThru, Rca, &DevStatus);
  if (EFI_ERROR(Status))
  {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardSendStatus failed with status %r\n", Status));
    return Status;
  }

  if (DevStatus & BIT7) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: Got switch error, DevStatus is 0x%x\n", DevStatus));
    return EFI_DEVICE_ERROR;
  }

  if (Private->SdInfo.CardType == SdCard) {
    DEBUG ((DEBUG_INFO, "MsdcDxe: SD host %u using %u-bit data at up to %u Hz\n",
            Private->Index, BusWidth, FixedPcdGet32 (PcdMsdcRemovableMaxClockHz)));
    return MsdcSetMclk (Private, FixedPcdGet32 (PcdMsdcRemovableMaxClockHz));
  }

  DEBUG ((DEBUG_INFO, "MsdcDxe: eMMC host %u using %u-bit data at 50000000 Hz\n",
          Private->Index, BusWidth));
  return MsdcSetMclk (Private, 50 * 1000 * 1000);
}

EFI_STATUS
EmmcIdentification (
  MSDC_PRIVATE_DATA *Private)
{
  EFI_STATUS Status;
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru;
  UINT32 Ocr;
  UINTN Retry;

  PassThru = &Private->PassThru;

  Status = PowerControl (Private->Index, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: failed to power eMMC host %u: %r\n", Private->Index, Status));
    return Status;
  }

  MicroSecondDelay (20000);
  Status = MsdcSetMclk (Private, 400000);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardReset (PassThru);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardReset failed with status %r\n", Status));
    return Status;
  }

  Ocr = 0;

  for (Retry = 0; Retry < MSDC_OCR_RETRY_COUNT_EMMC; Retry++) {
    Status = EMMCSendOpCond (PassThru, Ocr, TRUE, &Ocr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: EMMCSendOpCond failed with status %r\n", Status));
      return Status;
    }

    if ((Ocr & BIT31) != 0) {
      break;
    }

    MicroSecondDelay (100000);
  }

  if ((Ocr & BIT31) == 0) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: eMMC OCR-ready timed out on host %u\n", Private->Index));
    return EFI_TIMEOUT;
  }

  Status = CardAllSendCid (PassThru);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardAllSendCid failed with status %r\n", Status));
    return Status;
  }

  Status = EMMCSetRca (PassThru, 1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: EMMCSetRca failed with status %r\n", Status));
    return Status;
  }

  Private->SdInfo.CardType = EmmcCard;
  DEBUG ((DEBUG_ERROR, "MsdcDxe: Found EMMC device at controller with index %d\n", Private->Index));

  Status = CardSetBusMode (Private, 1);

  return Status;
}

EFI_STATUS
SdCardIdentification (
  MSDC_PRIVATE_DATA *Private)
{
  EFI_STATUS Status;
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru;
  UINT32 Ocr;
  UINT16 Rca;
  BOOLEAN S18r;
  BOOLEAN Xpc;
  BOOLEAN Hcs;
  UINTN Retry;

  PassThru = &Private->PassThru;

  Status = PowerControl (Private->Index, TRUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: failed to power SD host %u: %r\n", Private->Index, Status));
    return Status;
  }

  MicroSecondDelay (20000);
  Status = MsdcSetMclk (Private, 400000);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardReset (PassThru);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardReset failed with status %r\n", Status));
    return Status;
  }

  MicroSecondDelay (10000);

  Status = SdSendIfCond (PassThru);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: SdSendIfCond failed with status %r\n", Status));
    return Status;
  }

  Status = SdCardSendOpCond (PassThru, 0, 0, FALSE, FALSE, FALSE, &Ocr);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: SdCardSendOpCond failed with status %r\n", Status));
    return Status;
  }

  S18r = FALSE;
  Xpc = TRUE;
  Hcs = TRUE;

  for (Retry = 0; Retry < MSDC_OCR_RETRY_COUNT_SD; Retry++) {
    Status = SdCardSendOpCond (PassThru, 0, Ocr, S18r, Xpc, Hcs, &Ocr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: SdCardSendOpCond failed with status %r\n", Status));
      return Status;
    }
    if ((Ocr & BIT31) != 0) {
      break;
    }

    MicroSecondDelay (10000);
  }

  if ((Ocr & BIT31) == 0) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: SD OCR-ready timed out on host %u\n", Private->Index));
    return EFI_TIMEOUT;
  }

  Status = CardAllSendCid (PassThru);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardAllSendCid failed with status %r\n", Status));
    return Status;
  }

  Status = SdCardSetRca (PassThru, &Rca);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: CardAllSendCid failed with status %r\n", Status));
    return Status;
  }

  Private->SdInfo.CardType = SdCard;
  DEBUG ((DEBUG_ERROR, "MsdcDxe: Found SD device at controller with index %d\n", Private->Index));

  Status = CardSetBusMode (Private, Rca);

  return Status;
}

EFI_STATUS
InitMsdc (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  MSDC_PRIVATE_DATA* Private;
  MSDC_DEVICE_PATH* DevicePath;
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  CHAR8 MsdcName[11];
  BOOLEAN Present;

  Status = EFI_SUCCESS;
  if (mMsdcExitBootServicesEvent == NULL) {
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    MsdcExitBootServices,
                    NULL,
                    &gEfiEventExitBootServicesGuid,
                    &mMsdcExitBootServicesEvent
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: failed to create OS handoff event: %r\n", Status));
      return Status;
    }
  }

  for (UINTN i = 0; i < gPlatformInfo.NumberOfHosts; i++) {
    if ((FixedPcdGet32 (PcdMsdcHostMask) & (1U << i)) == 0) {
      continue;
    }

    Status = GetCardPresent (i, &Present);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: failed to read card detect for host %u: %r\n", i, Status));
      continue;
    }

    if (!Present) {
      DEBUG ((DEBUG_INFO, "MsdcDxe: no card present on removable host %u\n", i));
      continue;
    }

    Private = AllocateCopyPool (sizeof(MSDC_PRIVATE_DATA), &gMSDCPrivateDataTemplate);
    if (Private == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    DevicePath = AllocateCopyPool (sizeof(MSDC_DEVICE_PATH), &gMSDCDevicePathTemplate);
    if (DevicePath == NULL) {
      FreePool (Private);
      return EFI_OUT_OF_RESOURCES;
    }
    DevicePath->Mmc.Guid.Data4[7] = i;

    Private->Index = i;

    ZeroMem(MsdcName, sizeof(MsdcName));
    AsciiSPrint(MsdcName, sizeof(MsdcName), "MSDC-%u", i);

    Status = LocateMemoryRegionByName (MsdcName, &Region);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Failed to Locate %s Memory Region! Status = %r\n", MsdcName, Status));
      FreePool (DevicePath);
      FreePool (Private);
      continue;
    }

    Private->MsdcMmioReg = Region.Address;

    if (gPlatformInfo.UseTop) {
      ZeroMem(MsdcName, sizeof(MsdcName));
      AsciiSPrint(MsdcName, sizeof(MsdcName), "MSDC Top-%u", i);

      Status = LocateMemoryRegionByName (MsdcName, &Region);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "Failed to Locate %s Memory Region! Status = %r\n", MsdcName, Status));
        FreePool (DevicePath);
        FreePool (Private);
        continue;
      }

      Private->TopMmioReg = Region.Address;
    }

    Status = MsdcInit (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: failed to initialize host %u: %r\n", i, Status));
      if ((FixedPcdGet32 (PcdMsdcPreserveBootStateMask) & (1U << i)) == 0) {
        ClockControl (i, FALSE);
      }
      FreePool (DevicePath);
      FreePool (Private);
      continue;
    }

    // MTK host 0 is the non-removable eMMC when the platform advertises it;
    // all other hosts are removable SD. Never probe both card types per host.
    Status = ((i == 0) && FixedPcdGetBool (PcdStorageIsEMMC))
             ? EmmcIdentification (Private)
             : SdCardIdentification (Private);

    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "MsdcDxe: no usable card on host %u: %r\n", i, Status));
      if ((FixedPcdGet32 (PcdMsdcPreserveBootStateMask) & (1U << i)) == 0) {
        PowerControl (i, FALSE);
        ClockControl (i, FALSE);
      }
      FreePool (Private);
      FreePool (DevicePath);
      continue;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Private->ControllerHandle,
                    &gEfiDevicePathProtocolGuid, DevicePath,
                    &gEfiSdMmcPassThruProtocolGuid, &(Private->PassThru),
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      if ((FixedPcdGet32 (PcdMsdcPreserveBootStateMask) & (1U << i)) == 0) {
        PowerControl (i, FALSE);
        ClockControl (i, FALSE);
      }
      FreePool (DevicePath);
      FreePool (Private);
      continue;
    }

    if (i < ARRAY_SIZE (mMsdcHosts)) {
      mMsdcHosts[i] = Private;
    }
  }

  // A missing removable card or an unavailable host must not prevent BDS and
  // the firmware Shell from running. Successfully installed controllers are
  // represented by their pass-through handles.
  return EFI_SUCCESS;
}
