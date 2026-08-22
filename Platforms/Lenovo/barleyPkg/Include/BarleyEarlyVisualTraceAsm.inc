/*
 * Stackless Barley early framebuffer trace body.
 *
 * The caller supplies three 32-bit registers: stage, pattern A, pattern B.
 * x0 is never modified and is retained in the framebuffer diagnostic header.
 * Only caller-saved x8-x18 are used internally.
 */

#define BARLEY_TRACE_FB_BASE       0x7E605000
#define BARLEY_TRACE_FB_SIZE       0x017E8000
#define BARLEY_TRACE_FB_END        0x7FDED000
#define BARLEY_TRACE_BAND_BIT      17

.macro BARLEY_EARLY_VISUAL_TRACE stage_reg, pattern_a_reg, pattern_b_reg
  /* Capture entry architectural state before touching the framebuffer. */
  mrs   x12, CurrentEL
  mrs   x13, SCTLR_EL1
  mrs   x14, DAIF

  /* x8 = 0x7E605000, x10 = 0x017E8000; no literal pool or stack. */
  movz  x8, #0x5000
  movk  x8, #0x7E60, lsl #16
  movz  x10, #0x8000
  movk  x10, #0x017E, lsl #16
  mov   x9, xzr

.LBarleyTraceFill\@:
  /* Alternate 128 KiB high-contrast bands across the full reservation. */
  tst   x9, #0x20000
  csel  w17, \pattern_a_reg, \pattern_b_reg, eq
  stp   w17, w17, [x8], #8
  add   x9, x9, #8
  cmp   x9, x10
  b.lo  .LBarleyTraceFill\@

  /*
   * Fixed 48-byte diagnostic header at the start of the framebuffer:
   *   +0x00 magic "BTRC"       +0x04 stage
   *   +0x08 CurrentEL          +0x0C SCTLR_EL1[31:0]
   *   +0x10 SCTLR_EL1[63:32]   +0x14 DAIF[31:0]
   *   +0x18 DAIF[63:32]        +0x1C pattern A
   *   +0x20 pattern B          +0x28 incoming/current x0
   */
  movz  x8, #0x5000
  movk  x8, #0x7E60, lsl #16
  movz  w17, #0x5243
  movk  w17, #0x4254, lsl #16
  str   w17, [x8, #0x00]
  str   \stage_reg, [x8, #0x04]
  str   w12, [x8, #0x08]
  str   w13, [x8, #0x0C]
  lsr   x15, x13, #32
  str   w15, [x8, #0x10]
  str   w14, [x8, #0x14]
  lsr   x15, x14, #32
  str   w15, [x8, #0x18]
  str   \pattern_a_reg, [x8, #0x1C]
  str   \pattern_b_reg, [x8, #0x20]
  str   x0, [x8, #0x28]

  dsb   sy

  /* Test the SCTLR.C bit for the active exception level without altering it. */
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

  /* Clean every written cache line to PoC using CTR_EL0.DminLine. */
  mrs   x16, CTR_EL0
  ubfx  x16, x16, #16, #4
  mov   x17, #4
  lsl   x16, x17, x16
  sub   x17, x16, #1
  movz  x8, #0x5000
  movk  x8, #0x7E60, lsl #16
  bic   x8, x8, x17
  movz  x9, #0xD000
  movk  x9, #0x7FDE, lsl #16
.LBarleyTraceCacheLoop\@:
  dc    cvac, x8
  add   x8, x8, x16
  cmp   x8, x9
  b.lo  .LBarleyTraceCacheLoop\@

.LBarleyTraceCacheDone\@:
  dsb   sy
  isb
.endm
