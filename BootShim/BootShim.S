#if EARLY_VISUAL_TRACE == 1
#include "../Platforms/Lenovo/barleyPkg/Include/BarleyEarlyVisualTraceAsm.inc"
#endif

_Head:
  adr   x1, _Payload
  b     _BootShimEntry

#if REQUIRES_KERNEL_HEADER == 1
_StackBase:
  .quad KERNEL_HEADER_TEXT_OFFSET
_StackSize:
  .quad FD_SIZE
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .ascii "ARM\x64"
  .long 0
#endif

_BootShimEntry:
#if EARLY_VISUAL_TRACE == 1
  /* Stage 1: literal BootShim entry. */
  mov   w7, #1
  mov   w5, wzr
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
#endif

_Start:
  mov   x4, x1
  ldr   x5, =FD_BASE
  cmp   x4, x5
  beq   _Entry

#if EARLY_VISUAL_TRACE == 1
  /* Stage 2: immediately before FD relocation. */
  mov   w7, #2
  mov   w5, wzr
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
#endif

  /* Reload values after the marker's documented scratch-register use. */
  ldr   x5, =FD_BASE
  ldr   x6, =FD_SIZE

_CopyLoop:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  x6, x6, #16
  b.ne  _CopyLoop

#if EARLY_VISUAL_TRACE == 1
  /* Stage 3: complete FD copy. */
  mov   w7, #3
  mov   w5, wzr
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6

  /* Stage 4: immediately before the FD branch. */
  mov   w7, #4
  mov   w5, wzr
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
#endif

  ldr   x5, =FD_BASE
_Entry:
  br    x5

_Dead:
  b     _Dead

.text
.align 4
_Payload:
