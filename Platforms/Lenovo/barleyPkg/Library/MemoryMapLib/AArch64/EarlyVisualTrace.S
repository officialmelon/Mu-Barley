#include <AsmMacroLib.h>

.include "BarleyEarlyVisualTraceAsm.inc"

ASM_FUNC (BarleyEarlyVisualTrace)
  BARLEY_EARLY_VISUAL_TRACE w0, w1, w2
  ret

ASM_FUNC (BarleyTraceFdResetVector)
  mov   w7, #4
  movz  w5, #0xFFFF
  movk  w5, #0x00FF, lsl #16
  mov   w6, w5
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  ret

ASM_FUNC (BarleyTraceSecModuleEntry)
  mov   w7, #5
  movz  w5, #0x00FF
  movk  w5, #0x00FF, lsl #16
  mov   w6, w5
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  ret
