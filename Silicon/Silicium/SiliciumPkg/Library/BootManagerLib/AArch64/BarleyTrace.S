#include <AsmMacroLib.h>

#if BARLEY_STAGE_TRACE == 1
.include "BarleyEarlyVisualTraceAsm.inc"

ASM_FUNC (BarleyEarlyVisualTrace)
  BARLEY_EARLY_VISUAL_TRACE w0, w1, w2
  ret
#endif
