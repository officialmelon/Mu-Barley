#if EARLY_VISUAL_TRACE == 1
#include "../Platforms/Lenovo/barleyPkg/Include/BarleyEarlyVisualTraceAsm.inc"
#endif

_Head:
  adr   x1, _Payload    /* Set _Entry address */
  b     _BootShimEntry  /* Keep the ARM64 Image header at offset 0x08 */

#if REQUIRES_KERNEL_HEADER == 1
_StackBase:
  .quad KERNEL_HEADER_TEXT_OFFSET /* Linux arm64 Image text_offset */

_StackSize:
  .quad FD_SIZE         /* Image Size */
  .quad 0               /* Flags */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .ascii "ARM\x64"      /* ARM64 Magic */
  .long 0               /* Reserved */
#endif

_BootShimEntry:
#if EARLY_VISUAL_TRACE == 1
  /* Stage 1: red/black bands. x0 remains the unmodified LK handoff value. */
  mov   w7, #1
  movz  w5, #0x00FF, lsl #16
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  bl    _TraceDelay
#endif

_Start:
  mov   x4, x1
  ldr   x5, =FD_BASE
  cmp   x4, x5
  beq   _Entry
  ldr   x6,  =FD_SIZE

_CopyLoop:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  x6, x6, #16
  b.ne  _CopyLoop

#if EARLY_VISUAL_TRACE == 1
  /* Stage 2: green/black bands after the complete 2 MiB FD copy. */
  mov   w7, #2
  movz  w5, #0xFF00
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  bl    _TraceDelay

  /* Stage 3: blue/white bands immediately before the FD branch. */
  mov   w7, #3
  movz  w5, #0x00FF
  movz  w6, #0xFFFF
  movk  w6, #0x00FF, lsl #16
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  bl    _TraceDelay
#endif

  ldr   x5, =FD_BASE

_Entry:
  br    x5

_Dead:
  b     _Dead

#if EARLY_VISUAL_TRACE == 1
/* Deterministic stackless delay; no timer, interrupt, or firmware service. */
_TraceDelay:
  movz  x16, #0x1000, lsl #16
0:
  subs  x16, x16, #1
  b.ne  0b
  ret
#endif

.text
.align 4

_Payload:
