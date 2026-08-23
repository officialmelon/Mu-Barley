#ifndef BARLEY_LK_DISPLAY_H_
#define BARLEY_LK_DISPLAY_H_

#include <Uefi.h>

#define BARLEY_DISPLAY_WIDTH            1200U
#define BARLEY_DISPLAY_HEIGHT           1920U
#define BARLEY_DISPLAY_BYTES_PER_PIXEL  4U

#define BARLEY_FB0_BASE                 0x7BCE0000ULL
#define BARLEY_FB1_BASE                 0x7C5C8000ULL
#define BARLEY_FB2_BASE                 0x7CEB0000ULL
#define BARLEY_FB_CARVEOUT_END          0x7DC00000ULL
#define BARLEY_PITCH_PACKED             4800U
#define BARLEY_PITCH_ALIGNED            4864U

#endif
