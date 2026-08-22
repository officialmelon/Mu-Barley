/* M2.14 stackless execution trace in Lenovo's persistent ramoops console. */
#define BARLEY_PSTORE_CONSOLE_BASE  0x4D02F000
#define BARLEY_PSTORE_RECORD_SIZE   40

.macro BARLEY_EARLY_VISUAL_TRACE stage_reg, pattern_a_reg, pattern_b_reg
  /*
   * The Linux 6.6 persistent-RAM header is:
   *   u32 signature, atomic_t start, atomic_t size, u8 data[]
   * Lenovo's live parameters place the console zone at 0x4D02F000.
   * This leaf body uses only caller-saved x8-x18 and preserves incoming x0.
   */
  mrs   x12, CurrentEL
  mrs   x13, SCTLR_EL1
  mrs   x14, DAIF

  movz  x8, #0xF000
  movk  x8, #0x4D02, lsl #16

  /* BARLEY_PSTORE_TRACE_RECORD begins at persistent_ram_buffer.data. */
  movz  w17, #0x5442
  movk  w17, #0x4352, lsl #16       /* "BTRC" */
  str   w17, [x8, #12]
  movz  w17, #0x1400
  movk  w17, #0x0002, lsl #16       /* version 0x00021400 */
  str   w17, [x8, #16]
  str   \stage_reg, [x8, #20]
  str   w12, [x8, #24]
  str   w13, [x8, #28]
  lsr   x15, x13, #32
  str   w15, [x8, #32]
  str   w14, [x8, #36]
  lsr   x15, x14, #32
  str   w15, [x8, #40]
  str   x0, [x8, #44]

  /* Publish start/size before the signature, so partial records are invalid. */
  mov   w17, #40
  str   w17, [x8, #4]
  str   w17, [x8, #8]
  movz  w17, #0x4244
  movk  w17, #0x4347, lsl #16       /* Linux PERSISTENT_RAM_SIG "DBGC" */
  str   w17, [x8, #0]

  dsb   sy

  /* Match the proven CurrentEL-gated cache clean used by the prior trace. */
  and   x15, x12, #0xC
  cmp   x15, #0xC
  b.eq  .LBarleyPstoreReadEl3\@
  cmp   x15, #0x8
  b.eq  .LBarleyPstoreReadEl2\@
  mrs   x15, SCTLR_EL1
  b     .LBarleyPstoreCacheState\@
.LBarleyPstoreReadEl2\@:
  mrs   x15, SCTLR_EL2
  b     .LBarleyPstoreCacheState\@
.LBarleyPstoreReadEl3\@:
  mrs   x15, SCTLR_EL3
.LBarleyPstoreCacheState\@:
  tbz   x15, #2, .LBarleyPstoreCacheDone\@
  dc    cvac, x8
.LBarleyPstoreCacheDone\@:
  dsb   sy
  isb
.endm
