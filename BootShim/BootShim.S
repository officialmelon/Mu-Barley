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
  /* Stage 1: true black/white bands. x0 remains LK's handoff value. */
  mov   w7, #1
  mov   w5, wzr
  movz  w6, #0xFFFF
  movk  w6, #0x00FF, lsl #16
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  bl    _SnapshotLiveDisplay
  bl    _MarkLiveDisplaySources
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
  /* Stage 2: inverted white/black bands after the complete 2 MiB FD copy. */
  mov   w7, #2
  movz  w5, #0xFFFF
  movk  w5, #0x00FF, lsl #16
  mov   w6, wzr
  BARLEY_EARLY_VISUAL_TRACE w7, w5, w6
  bl    _TraceDelay

  /* Stage 3: black/white bands immediately before the FD branch. */
  mov   w7, #3
  mov   w5, wzr
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
/*
 * Capture the untouched LK display pipeline in the final protected page of
 * the M2.8 allocation.  MT6768 OVL addresses are at 0x0F40 + 0x20 * layer;
 * the remaining offsets match Lenovo LK's platform/mt6768 DDP drivers.
 */
_SnapshotLiveDisplay:
  movz  x8, #0xC000
  movk  x8, #0x7FDE, lsl #16
  movz  w10, #0x0901
  movk  w10, #0x0002, lsl #16
  str   w10, [x8, #0x30]

  /* RDMA0: GLOBAL, SIZE0/1, MEM_CON, PITCH, START_ADDR. */
  movz  x9, #0xD000
  movk  x9, #0x1400, lsl #16
  ldr   w10, [x9, #0x10]
  str   w10, [x8, #0x34]
  ldr   w10, [x9, #0x14]
  str   w10, [x8, #0x38]
  ldr   w10, [x9, #0x18]
  str   w10, [x8, #0x3C]
  ldr   w10, [x9, #0x24]
  str   w10, [x8, #0x40]
  ldr   w10, [x9, #0x2C]
  str   w10, [x8, #0x44]
  ldr   w10, [x9, #0xF00]
  str   w10, [x8, #0x48]

  /* OVL0 enable/source plus address, pitch, size and format for four layers. */
  movz  x9, #0xB000
  movk  x9, #0x1400, lsl #16
  ldr   w10, [x9, #0x0C]
  str   w10, [x8, #0x4C]
  ldr   w10, [x9, #0x2C]
  str   w10, [x8, #0x50]
  ldr   w10, [x9, #0xF40]
  str   w10, [x8, #0x54]
  ldr   w10, [x9, #0x44]
  str   w10, [x8, #0x58]
  ldr   w10, [x9, #0x38]
  str   w10, [x8, #0x5C]
  ldr   w10, [x9, #0x30]
  str   w10, [x8, #0x60]
  ldr   w10, [x9, #0xF60]
  str   w10, [x8, #0x64]
  ldr   w10, [x9, #0x64]
  str   w10, [x8, #0x68]
  ldr   w10, [x9, #0x58]
  str   w10, [x8, #0x6C]
  ldr   w10, [x9, #0x50]
  str   w10, [x8, #0x70]
  ldr   w10, [x9, #0xF80]
  str   w10, [x8, #0x74]
  ldr   w10, [x9, #0x84]
  str   w10, [x8, #0x78]
  ldr   w10, [x9, #0x78]
  str   w10, [x8, #0x7C]
  ldr   w10, [x9, #0x70]
  str   w10, [x8, #0x80]
  ldr   w10, [x9, #0xFA0]
  str   w10, [x8, #0x84]
  ldr   w10, [x9, #0xA4]
  str   w10, [x8, #0x88]
  ldr   w10, [x9, #0x98]
  str   w10, [x8, #0x8C]
  ldr   w10, [x9, #0x90]
  str   w10, [x8, #0x90]

  /* OVL0_2L has the same MT6768 layout but only two physical layers. */
  movz  x9, #0xC000
  movk  x9, #0x1400, lsl #16
  ldr   w10, [x9, #0x0C]
  str   w10, [x8, #0x94]
  ldr   w10, [x9, #0x2C]
  str   w10, [x8, #0x98]
  ldr   w10, [x9, #0xF40]
  str   w10, [x8, #0x9C]
  ldr   w10, [x9, #0x44]
  str   w10, [x8, #0xA0]
  ldr   w10, [x9, #0x38]
  str   w10, [x8, #0xA4]
  ldr   w10, [x9, #0x30]
  str   w10, [x8, #0xA8]
  ldr   w10, [x9, #0xF60]
  str   w10, [x8, #0xAC]
  ldr   w10, [x9, #0x64]
  str   w10, [x8, #0xB0]
  ldr   w10, [x9, #0x58]
  str   w10, [x8, #0xB4]
  ldr   w10, [x9, #0x50]
  str   w10, [x8, #0xB8]

  /* Raw routing/clock state and DSI0 control state; no register is written. */
  movz  x9, #0x0000
  movk  x9, #0x1400, lsl #16
  ldr   w10, [x9, #0x100]
  str   w10, [x8, #0xBC]
  ldr   w10, [x9, #0x110]
  str   w10, [x8, #0xC0]
  movz  x9, #0x4000
  movk  x9, #0x1401, lsl #16
  ldr   w10, [x9, #0x00]
  str   w10, [x8, #0xC4]
  ldr   w10, [x9, #0x04]
  str   w10, [x8, #0xC8]
  ldr   w10, [x9, #0x10]
  str   w10, [x8, #0xCC]
  ldr   w10, [x9, #0x14]
  str   w10, [x8, #0xD0]
  ldr   w10, [x9, #0x18]
  str   w10, [x8, #0xD4]
  ldr   w10, [x9, #0x1C]
  str   w10, [x8, #0xD8]
  dsb   sy
  ret

/* Mark every enabled physical-memory input that is plausibly in LK display RAM. */
_MarkLiveDisplaySources:
  mov   x21, x30
  movz  x20, #0xC000
  movk  x20, #0x7FDE, lsl #16
  ldr   w11, [x20, #0x4C]
  tbz   w11, #0, 9f
  ldr   w23, [x20, #0x50]
  tbz   w23, #0, 1f
  ldr   w8, [x20, #0x54]
  bl    _FillLiveDisplaySource
1:
  tbz   w23, #1, 2f
  ldr   w8, [x20, #0x64]
  bl    _FillLiveDisplaySource
2:
  tbz   w23, #2, 3f
  ldr   w8, [x20, #0x74]
  bl    _FillLiveDisplaySource
3:
  tbz   w23, #3, 9f
  ldr   w8, [x20, #0x84]
  bl    _FillLiveDisplaySource
9:
  ldr   w11, [x20, #0x94]
  tbz   w11, #0, 19f
  ldr   w23, [x20, #0x98]
  tbz   w23, #0, 11f
  ldr   w8, [x20, #0x9C]
  bl    _FillLiveDisplaySource
11:
  tbz   w23, #1, 19f
  ldr   w8, [x20, #0xAC]
  bl    _FillLiveDisplaySource
19:
  /* RDMA address is valid only when GLOBAL_CON.memory_mode (bit 1) is set. */
  ldr   w11, [x20, #0x34]
  tbz   w11, #1, 20f
  ldr   w8, [x20, #0x48]
  bl    _FillLiveDisplaySource
20:
  mov   x30, x21
  ret

_FillLiveDisplaySource:
  /* Constrain dynamic stores to Lenovo's verified low-DRAM display window. */
  mov   x18, x8
  movz  x11, #0x0000
  movk  x11, #0x7000, lsl #16
  cmp   x8, x11
  b.lo  29f
  movz  x11, #0x0000
  movk  x11, #0x8000, lsl #16
  cmp   x8, x11
  b.hs  29f
  movz  x10, #0x8000
  movk  x10, #0x008E, lsl #16
  add   x15, x8, x10
  cmp   x15, x11
  b.hi  29f
  mov   x9, xzr
21:
  tst   x9, #0x10000
  csel  w17, w5, w6, eq
  stp   w17, w17, [x8], #8
  add   x9, x9, #8
  cmp   x9, x10
  b.lo  21b
  dsb   sy

  mrs   x12, CurrentEL
  and   x15, x12, #0xC
  cmp   x15, #0xC
  b.eq  24f
  cmp   x15, #0x8
  b.eq  23f
  mrs   x15, SCTLR_EL1
  b     25f
23:
  mrs   x15, SCTLR_EL2
  b     25f
24:
  mrs   x15, SCTLR_EL3
25:
  tbz   x15, #2, 28f
  mrs   x16, CTR_EL0
  ubfx  x16, x16, #16, #4
  mov   x17, #4
  lsl   x16, x17, x16
  sub   x17, x16, #1
  bic   x18, x18, x17
26:
  dc    cvac, x18
  add   x18, x18, x16
  cmp   x18, x8
  b.lo  26b
28:
  dsb   sy
  isb
29:
  ret

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
