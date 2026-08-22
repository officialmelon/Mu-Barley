#include <AsmMacroLib.h>

.include "BarleyEarlyVisualTraceAsm.inc"

ASM_FUNC (BarleyEarlyVisualTrace)
  BARLEY_EARLY_VISUAL_TRACE w0, w1, w2
  ret
