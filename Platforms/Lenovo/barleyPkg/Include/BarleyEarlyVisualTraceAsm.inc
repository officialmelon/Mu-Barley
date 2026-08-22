/* M2.14 stackless execution trace in Lenovo's persistent ramoops console. */
#define BARLEY_PSTORE_CONSOLE_BASE  0x4D02F000
#define BARLEY_PSTORE_DATA_SIZE     0x0003FFF4

.macro BARLEY_PSTORE_WRITE_U64
  .rept 8
    strb  w15, [x11, x9]
    lsr   x15, x15, #8
    add   x9, x9, #1
    cmp   x9, x10
    csel  x9, xzr, x9, eq
  .endr
.endm

.macro BARLEY_EARLY_VISUAL_TRACE stage_reg, pattern_a_reg, pattern_b_reg
  /*
   * The Linux 6.6 persistent-RAM header is:
   *   u32 signature, atomic_t start, atomic_t size, u8 data[]
   * Lenovo's live parameters place the console zone at 0x4D02F000.
   * This leaf body uses only caller-saved x8-x17 and preserves incoming x0.
   */
  mrs   x12, CurrentEL
  mrs   x13, SCTLR_EL1
  mrs   x14, DAIF

  /* Pack the record before x12 is reused for the original ring position. */
  movz  x16, #0x5442
  movk  x16, #0x4352, lsl #16       /* "BTRC" */
  movk  x16, #0x1403, lsl #32
  movk  x16, #0x0002, lsl #48       /* version 0x00021403 */
  uxtw  x17, \stage_reg
  bfi   x17, x12, #32, #32          /* stage, CurrentEL */

  movz  x8, #0xF000
  movk  x8, #0x4D02, lsl #16
  movz  x10, #0xFFF4
  movk  x10, #0x0003, lsl #16       /* 0x40000 - 12-byte header */

  /*
   * Append using Linux persistent_ram_write() ring semantics.  M2.14.2
   * wrote data[0] without advancing start/size, so Linux correctly ignored
   * it.  Byte stores are intentional: LK console writes need not leave the
   * ring cursor naturally aligned, and pre-stack unaligned word stores fault.
   */
  ldr   w15, [x8, #0]
  movz  w11, #0x4244
  movk  w11, #0x4347, lsl #16       /* PERSISTENT_RAM_SIG "DBGC" */
  cmp   w15, w11
  b.ne  .LBarleyPstoreResetRing\@
  ldr   w9, [x8, #4]
  ldr   w15, [x8, #8]
  cmp   x9, x10
  b.hs  .LBarleyPstoreResetRing\@
  cmp   x15, x10
  b.hi  .LBarleyPstoreResetRing\@
  cmp   x9, x15
  b.hi  .LBarleyPstoreResetRing\@
  b     .LBarleyPstoreRingReady\@
.LBarleyPstoreResetRing\@:
  mov   x9, xzr
  mov   x15, xzr
  str   w9, [x8, #4]
  str   w15, [x8, #8]
  str   w11, [x8, #0]
.LBarleyPstoreRingReady\@:
  mov   x12, x9                       /* original append position */
  add   x11, x8, #12                  /* persistent_ram_buffer.data */

  mov   x15, x16
  BARLEY_PSTORE_WRITE_U64
  mov   x15, x17
  BARLEY_PSTORE_WRITE_U64
  mov   x15, x13
  BARLEY_PSTORE_WRITE_U64
  mov   x15, x14
  BARLEY_PSTORE_WRITE_U64
  mov   x15, x0
  BARLEY_PSTORE_WRITE_U64

  /* Publish size first, then the next-write cursor, matching Linux ordering. */
  ldr   w15, [x8, #8]
  add   x15, x15, #40
  cmp   x15, x10
  csel  x15, x10, x15, hi
  str   w15, [x8, #8]
  str   w9, [x8, #4]

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
  dc    cvac, x8                       /* signature/start/size */
  add   x16, x11, x12
  dc    cvac, x16                      /* first record cache line */
  add   x17, x16, #39
  dc    cvac, x17                      /* possible second cache line */
  dc    cvac, x11                      /* wrapped record tail, if any */
.LBarleyPstoreCacheDone\@:
  dsb   sy
  isb
.endm
