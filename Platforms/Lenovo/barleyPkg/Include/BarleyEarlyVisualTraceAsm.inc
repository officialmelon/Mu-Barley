/*
 * Stackless Barley early framebuffer trace body.
 *
 * The caller supplies three 32-bit registers: stage, pattern A, pattern B.
 * x0 is never modified and is retained in the protected diagnostic page.
 * Only caller-saved x8-x18 are used internally.
 */

#define BARLEY_TRACE_LOGO_BASE       0x7A3F8000
#define BARLEY_TRACE_LOGO_SIZE       0x008CA000
#define BARLEY_TRACE_FB_BASE         0x7E605000
#define BARLEY_TRACE_FB_SIZE         0x017E7000
#define BARLEY_TRACE_DIAG_BASE       0x7FDEC000
#define BARLEY_TRACE_BAND_BIT        17

.macro BARLEY_TRACE_FILL base_hi, base_lo, size_hi, size_lo, pattern_a_reg, pattern_b_reg
  movz  x8, #\base_lo
  movk  x8, #\base_hi, lsl #16
  movz  x10, #\size_lo
  movk  x10, #\size_hi, lsl #16
  mov   x9, xzr
.LBarleyTraceFill\@:
  /* Alternate genuine black/white 128 KiB bands across the range. */
  tst   x9, #0x20000
  csel  w17, \pattern_a_reg, \pattern_b_reg, eq
  stp   w17, w17, [x8], #8
  add   x9, x9, #8
  cmp   x9, x10
  b.lo  .LBarleyTraceFill\@
.endm

.macro BARLEY_TRACE_CLEAN base_hi, base_lo, end_hi, end_lo
  movz  x8, #\base_lo
  movk  x8, #\base_hi, lsl #16
  bic   x8, x8, x17
  movz  x9, #\end_lo
  movk  x9, #\end_hi, lsl #16
.LBarleyTraceCacheLoop\@:
  dc    cvac, x8
  add   x8, x8, x16
  cmp   x8, x9
  b.lo  .LBarleyTraceCacheLoop\@
.endm

.macro BARLEY_EARLY_VISUAL_TRACE stage_reg, pattern_a_reg, pattern_b_reg
  /* Capture entry architectural state before touching either surface. */
  mrs   x12, CurrentEL
  mrs   x13, SCTLR_EL1
  mrs   x14, DAIF

  /* Lenovo logo decompression output: 1200 * 1920 * 4 exactly. */
  BARLEY_TRACE_FILL 0x7A3F, 0x8000, 0x008C, 0xA000, \pattern_a_reg, \pattern_b_reg

  /* M2.8 FDT allocation, excluding only its protected final diagnostic page. */
  BARLEY_TRACE_FILL 0x7E60, 0x5000, 0x017E, 0x7000, \pattern_a_reg, \pattern_b_reg

  /*
   * Fixed diagnostic page at 0x7FDEC000:
   *   +0x00 magic "BTRC"       +0x04 stage
   *   +0x08 CurrentEL          +0x0C SCTLR_EL1[31:0]
   *   +0x10 SCTLR_EL1[63:32]   +0x14 DAIF[31:0]
   *   +0x18 DAIF[63:32]        +0x1C pattern A
   *   +0x20 pattern B          +0x28 incoming/current x0
   * Hardware state captured by the first BootShim point starts at +0x30.
   */
  movz  x8, #0xC000
  movk  x8, #0x7FDE, lsl #16
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
  BARLEY_TRACE_CLEAN 0x7A3F, 0x8000, 0x7ACC, 0x2000
  BARLEY_TRACE_CLEAN 0x7E60, 0x5000, 0x7FDE, 0xC000
  BARLEY_TRACE_CLEAN 0x7FDE, 0xC000, 0x7FDE, 0xD000

.LBarleyTraceCacheDone\@:
  dsb   sy
  isb
.endm
