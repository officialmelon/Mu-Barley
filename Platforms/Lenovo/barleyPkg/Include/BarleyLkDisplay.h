#ifndef BARLEY_LK_DISPLAY_H_
#define BARLEY_LK_DISPLAY_H_

#include <Uefi.h>

#define BARLEY_DISPLAY_WIDTH                  1200U
#define BARLEY_DISPLAY_HEIGHT                 1920U
#define BARLEY_DISPLAY_BYTES_PER_PIXEL        4U

#define BARLEY_LK_LOGO_BASE                   0x7A3F8000ULL
#define BARLEY_LK_LOGO_SIZE                   0x008CA000ULL
#define BARLEY_LK_LOGO_ALLOCATION_SIZE        0x008E8000ULL
#define BARLEY_LK_FRAMEBUFFER_BASE            0x7BCE0000ULL
#define BARLEY_LK_FRAMEBUFFER_SIZE            0x01F20000ULL
#define BARLEY_FDT_DISPLAY_BASE               0x7E605000ULL
#define BARLEY_FDT_DISPLAY_SIZE               0x017E8000ULL
#define BARLEY_DISPLAY_DIAGNOSTIC_BASE        0x7FDEC000ULL
#define BARLEY_DISPLAY_DIAGNOSTIC_SIZE        0x00001000ULL

#define BARLEY_MMSYS_BASE                     0x14000000U
#define BARLEY_OVL0_BASE                      0x1400B000U
#define BARLEY_OVL0_2L_BASE                   0x1400C000U
#define BARLEY_RDMA0_BASE                     0x1400D000U
#define BARLEY_DSI0_BASE                      0x14014000U

// MT6768 display register offsets, matching Lenovo LK's DDP implementation.
#define BARLEY_OVL_EN                         0x000CU
#define BARLEY_OVL_ROI_SIZE                   0x0020U
#define BARLEY_OVL_SRC_CON                    0x002CU
#define BARLEY_OVL_L_CON(Layer)               (0x0030U + (0x20U * (Layer)))
#define BARLEY_OVL_L_SRC_SIZE(Layer)          (0x0038U + (0x20U * (Layer)))
#define BARLEY_OVL_L_PITCH(Layer)             (0x0044U + (0x20U * (Layer)))
#define BARLEY_OVL_L_ADDR(Layer)              (0x0F40U + (0x20U * (Layer)))

#define BARLEY_RDMA_GLOBAL_CON                0x0010U
#define BARLEY_RDMA_SIZE_CON_0                0x0014U
#define BARLEY_RDMA_SIZE_CON_1                0x0018U
#define BARLEY_RDMA_MEM_CON                   0x0024U
#define BARLEY_RDMA_MEM_SRC_PITCH             0x002CU
#define BARLEY_RDMA_MEM_START_ADDR            0x0F00U
#define BARLEY_RDMA_MEMORY_MODE               BIT1

#define BARLEY_DISPLAY_DIAGNOSTIC_MAGIC       SIGNATURE_32 ('B', 'T', 'R', 'C')
#define BARLEY_DISPLAY_DIAGNOSTIC_VERSION     0x00020901U

#pragma pack (1)
typedef struct {
  UINT32 Magic;
  UINT32 Stage;
  UINT32 CurrentEl;
  UINT32 SctlrEl1Low;
  UINT32 SctlrEl1High;
  UINT32 DaifLow;
  UINT32 DaifHigh;
  UINT32 PatternA;
  UINT32 PatternB;
  UINT32 Reserved24;
  UINT64 IncomingX0;
  UINT32 Version;
  UINT32 RdmaGlobalCon;
  UINT32 RdmaSizeCon0;
  UINT32 RdmaSizeCon1;
  UINT32 RdmaMemCon;
  UINT32 RdmaMemSrcPitch;
  UINT32 RdmaMemStartAddr;
  UINT32 Ovl0En;
  UINT32 Ovl0SrcCon;
  struct {
    UINT32 Address;
    UINT32 Pitch;
    UINT32 Size;
    UINT32 Control;
  } Ovl0Layer[4];
  UINT32 Ovl02lEn;
  UINT32 Ovl02lSrcCon;
  struct {
    UINT32 Address;
    UINT32 Pitch;
    UINT32 Size;
    UINT32 Control;
  } Ovl02lLayer[2];
  UINT32 MmsysCgCon0;
  UINT32 MmsysCgCon1;
  UINT32 DsiStart;
  UINT32 DsiStatus;
  UINT32 DsiConCtrl;
  UINT32 DsiModeCtrl;
  UINT32 DsiTxrxCtrl;
  UINT32 DsiPsCtrl;
} BARLEY_DISPLAY_DIAGNOSTIC;
#pragma pack ()

STATIC_ASSERT (
  OFFSET_OF (BARLEY_DISPLAY_DIAGNOSTIC, RdmaGlobalCon) == 0x34,
  "BootShim diagnostic layout mismatch"
  );
STATIC_ASSERT (
  OFFSET_OF (BARLEY_DISPLAY_DIAGNOSTIC, Ovl0Layer[0].Address) == 0x54,
  "BootShim OVL0 diagnostic layout mismatch"
  );
STATIC_ASSERT (
  OFFSET_OF (BARLEY_DISPLAY_DIAGNOSTIC, Ovl02lLayer[0].Address) == 0x9C,
  "BootShim OVL0_2L diagnostic layout mismatch"
  );

#endif
