#ifndef _MSDC_IMPL_LIB_H_
#define _MSDC_IMPL_LIB_H_

typedef struct {
  UINT8 NumberOfHosts;
  BOOLEAN UseTop;
  UINT32 MsdcPadTuneReg;
  UINT32 TuningStep[2];
  BOOLEAN AsyncFifo;
  BOOLEAN BusyCheck;
  BOOLEAN StopClkFix;
  BOOLEAN EnhanceRx;
} MSDC_PLATFORM_INFO;

EFI_STATUS
GetSourceClockRate (
  IN  UINT32 Index,
  OUT UINTN *Hz
  );

EFI_STATUS
SourceClockControl (
  IN UINT32  Index,
  IN BOOLEAN Enable
  );

EFI_STATUS
ClockControl (
  IN UINT32  Index,
  IN BOOLEAN Enable
  );

EFI_STATUS
PowerControl (
  IN UINT32  Index,
  IN BOOLEAN Enable
  );

EFI_STATUS
InitGpio (
  IN UINT32 Index
  );

extern MSDC_PLATFORM_INFO gPlatformInfo;

#endif /* _MSDC_IMPL_LIB_H_ */
