// Lenovo LK has been physically verified to enter the payload at EL1.  Its
// 64-bit Linux handoff places the patched FDT pointer in x0.  Preserve that
// pointer before the common SEC entry code reuses x0 for EL setup.
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include <AsmMacroLib.h>

ASM_FUNC (PlatformAssemblyInitialize)
  MOV64 (x1, 0x40040000)
  MOV64 (x2, 0x2130585444464B4C)
  stp   x2, x0, [x1]
  dc    cvac, x1
  dsb   sy
  ret
