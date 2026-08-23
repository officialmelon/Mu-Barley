#ifndef BARLEY_LK_DISPLAY_H_
#define BARLEY_LK_DISPLAY_H_

#include <Uefi.h>

#define BARLEY_DISPLAY_WIDTH            1200U
#define BARLEY_DISPLAY_HEIGHT           1920U
#define BARLEY_DISPLAY_BYTES_PER_PIXEL  4U

#define BARLEY_OVL0_BASE                0x1400B000U
#define BARLEY_OVL0_2L_BASE             0x1400C000U
#define BARLEY_RDMA0_BASE               0x1400D000U

// MT6768 display register offsets used read-only by the passive LK GOP.
#define BARLEY_OVL_EN                    0x000CU
#define BARLEY_OVL_SRC_CON               0x002CU
#define BARLEY_OVL_L_SRC_SIZE(Layer)     (0x0038U + (0x20U * (Layer)))
#define BARLEY_OVL_L_PITCH(Layer)        (0x0044U + (0x20U * (Layer)))
#define BARLEY_OVL_L_ADDR(Layer)         (0x0F40U + (0x20U * (Layer)))

#define BARLEY_RDMA_GLOBAL_CON           0x0010U
#define BARLEY_RDMA_MEM_SRC_PITCH        0x002CU
#define BARLEY_RDMA_MEM_START_ADDR       0x0F00U
#define BARLEY_RDMA_MEMORY_MODE          BIT1

#endif
