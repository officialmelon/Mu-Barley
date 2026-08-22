/* M2.13 stackless execution-stage marker: one 256x256 LK logo block. */
#define BARLEY_TRACE_BASE       0x7A3F8000
#define BARLEY_TRACE_PITCH      4800
#define BARLEY_TRACE_DIMENSION  256
#define BARLEY_TRACE_DIAG_BASE  0x7FDEC000

.macro BARLEY_EARLY_VISUAL_TRACE stage_reg, pattern_a_reg, pattern_b_reg
  /* Leaf body: caller-saved x8-x18 only; incoming x0 is never modified. */
  mrs   x12, CurrentEL
  mrs   x13, SCTLR_EL1
  mrs   x14, DAIF

  movz  x8, #0xC000
  movk  x8, #0x7FDE, lsl #16
  movz  w17, #0x3231
  movk  w17, #0x334D, lsl #16       /* "M312" */
  str   w17, [x8, #0x00]
  str   \stage_reg, [x8, #0x04]
  str   w12, [x8, #0x08]
  str   w13, [x8, #0x0C]
  lsr   x15, x13, #32
  str   w15, [x8, #0x10]
  str   w14, [x8, #0x14]
  lsr   x15, x14, #32
  str   w15, [x8, #0x18]
  str   x0, [x8, #0x20]

  /* Clear the complete marker block to opaque black. */
  movz  x8, #0x8000
  movk  x8, #0x7A3F, lsl #16
  mov   x10, x8
  mov   x15, #4800
  mov   x9, #256
  mov   w17, wzr
  movk  w17, #0xFF00, lsl #16       /* 0xFF000000 */
.LBarleyTraceClearRow\@:
  mov   x11, x10
  mov   x16, #128
.LBarleyTraceClearPixel\@:
  stp   w17, w17, [x11], #8
  subs  x16, x16, #1
  b.ne  .LBarleyTraceClearPixel\@
  add   x10, x10, x15
  subs  x9, x9, #1
  b.ne  .LBarleyTraceClearRow\@

  /* Stage N is N eight-pixel vertical bars separated by eight pixels. */
  movz  w17, #0xFFFF
  movk  w17, #0xFFFF, lsl #16       /* 0xFFFFFFFF */
  uxtw  x18, \stage_reg
  mov   x14, xzr
.LBarleyTraceVerticalNext\@:
  cmp   x14, x18
  b.hs  .LBarleyTraceHorizontalStart\@
  movz  x10, #0x8000
  movk  x10, #0x7A3F, lsl #16
  add   x10, x10, x14, lsl #6
  mov   x9, #192
.LBarleyTraceVerticalRow\@:
  stp   w17, w17, [x10, #0]
  stp   w17, w17, [x10, #8]
  stp   w17, w17, [x10, #16]
  stp   w17, w17, [x10, #24]
  add   x10, x10, x15
  subs  x9, x9, #1
  b.ne  .LBarleyTraceVerticalRow\@
  add   x14, x14, #1
  b     .LBarleyTraceVerticalNext\@

  /* CurrentEL/4 horizontal bars occupy rows 208..255. */
.LBarleyTraceHorizontalStart\@:
  ubfx  x18, x12, #2, #2
  mov   x14, xzr
.LBarleyTraceHorizontalNext\@:
  cmp   x14, x18
  b.hs  .LBarleyTraceStoresDone\@
  mov   x10, #208
  add   x10, x10, x14, lsl #4
  mul   x10, x10, x15
  movz  x11, #0x8000
  movk  x11, #0x7A3F, lsl #16
  add   x10, x10, x11
  mov   x9, #8
.LBarleyTraceHorizontalRow\@:
  mov   x11, x10
  mov   x16, #128
.LBarleyTraceHorizontalPixel\@:
  stp   w17, w17, [x11], #8
  subs  x16, x16, #1
  b.ne  .LBarleyTraceHorizontalPixel\@
  add   x10, x10, x15
  subs  x9, x9, #1
  b.ne  .LBarleyTraceHorizontalRow\@
  add   x14, x14, #1
  b     .LBarleyTraceHorizontalNext\@

.LBarleyTraceStoresDone\@:
  dsb   sy

  /* Retain M2.9's cache-state gate and dc cvac clean-to-PoC operation. */
  and   x15, x12, #0xC
  cmp   x15, #0xC
  b.eq  .LBarleyTraceReadEl3\@
  cmp   x15, #0x8
  b.eq  .LBarleyTraceReadEl2\@
  mrs   x15, SCTLR_EL1
  b     .LBarleyTraceCacheState\@
.LBarleyTraceReadEl2\@:
  mrs   x15, SCTLR_EL2
  b     .LBarleyTraceCacheState\@
.LBarleyTraceReadEl3\@:
  mrs   x15, SCTLR_EL3
.LBarleyTraceCacheState\@:
  tbz   x15, #2, .LBarleyTraceCacheDone\@
  mrs   x16, CTR_EL0
  ubfx  x16, x16, #16, #4
  mov   x17, #4
  lsl   x16, x17, x16
  sub   x17, x16, #1

  movz  x8, #0x8000
  movk  x8, #0x7A3F, lsl #16
  bic   x8, x8, x17
  movz  x9, #0x4000
  movk  x9, #0x7A52, lsl #16        /* base + 4800 * 256 */
.LBarleyTraceCacheLoop\@:
  dc    cvac, x8
  add   x8, x8, x16
  cmp   x8, x9
  b.lo  .LBarleyTraceCacheLoop\@

  movz  x8, #0xC000
  movk  x8, #0x7FDE, lsl #16
  bic   x8, x8, x17
  movz  x9, #0xD000
  movk  x9, #0x7FDE, lsl #16
.LBarleyTraceDiagCacheLoop\@:
  dc    cvac, x8
  add   x8, x8, x16
  cmp   x8, x9
  b.lo  .LBarleyTraceDiagCacheLoop\@

.LBarleyTraceCacheDone\@:
  dsb   sy
  isb
.endm
