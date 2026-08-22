#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryMapLib.h>

#include <BarleyEarlyVisualTrace.h>

#define BARLEY_LIVE_FDT_BASE       0x4BC80000ULL
#define BARLEY_LIVE_FDT_MAX_SIZE   0x00080000U
#define BARLEY_DRAM_BASE           0x40000000ULL
#define BARLEY_DRAM_SIZE           0x200000000ULL
#define BARLEY_RESERVED_NODE_COUNT 23U
#define BARLEY_NO_MAP_NODE_COUNT   18U

STATIC EFI_MEMORY_REGION_DESCRIPTOR gBaseMemoryDescriptor[] = {
  // Name, Address, Length, HobOption, ResourceType, ResourceAttribute, MemoryType, ArmAttribute
  {"UEFI Stack",         0x40000000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  // Keep the FD outside LK's 0x40080000 kernel decompression address so the
  // BootShim never overwrites the instructions it is actively executing.
  {"UEFI FD",            0x4BD00000, 0x00200000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  // Stable across both live LK mblock captures; used only for the SEC/DXE HOB heap.
  {"DXE Heap",           0x56000000, 0x16000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK},
  // Actual LK scanout, not the distinct mblock-17-framebuffer carveout.
  {"Display Reserved",   0x7E605000, 0x017E8000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH},

  // MT6768 register regions verified against Barley firmware/device-tree evidence.
  {"GIC Distributor",    0x0C000000, 0x00040000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"GIC Redistributors", 0x0C040000, 0x00200000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Top Ck Gen",         0x10000000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Infra Cfg AO",       0x10001000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Pinctrl",            0x10005000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"WatchDog Timer",     0x10007000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"AP Mixed",           0x1000C000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"PMIC Wrapper",       0x1000D000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"EMI",                0x10219000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC-0",             0x11230000, 0x00010000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC-1",             0x11240000, 0x00010000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC Top-0",         0x11C90000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC Top-1",         0x11CD0000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display OVL",        0x1400B000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}
};

// Free mblocks stable byte-for-byte across both physical-device captures.
// Gaps deliberately preserve every LK reservation and transient boot allocation.
STATIC CONST EFI_MEMORY_REGION_DESCRIPTOR gStableMblocks[] = {
  // The first 2 MiB of the original mblock is now occupied by the relocated FD.
  {"LK Mblock 02", 0x4BF00000, 0x00180000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 03", 0x4C280000, 0x00180000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 04", 0x4C800000, 0x00600000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 05", 0x4CE60000, 0x001A0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 06", 0x4D100000, 0x00F00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 08", 0x6C900000, 0x03700000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 09", 0x73C00000, 0x043F0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 10", 0x79600000, 0x00DF8000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  // Mblock 11 is omitted because it overlaps the independent LK scanout allocation.
  {"LK Mblock 12", 0x80000000, 0x1F900000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 13", 0x9FF00000, 0x000F0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 14", 0xA0000000, 0x02000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 15", 0xB2000000, 0x01000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 16", 0xB7000000, 0x08C00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 17", 0xBFC20000, 0x001E0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 18", 0xBFE40000, 0x00140000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 19", 0xC0000000, 0x7FFFF000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 20", 0x140000000, 0xFFFFF000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK}
};

STATIC EFI_MEMORY_REGION_DESCRIPTOR gMemoryDescriptor[
  ARRAY_SIZE (gBaseMemoryDescriptor) + ARRAY_SIZE (gStableMblocks)
];
STATIC UINT8   gMemoryDescriptorCount;
STATIC BOOLEAN gMemoryMapInitialized;

STATIC
BOOLEAN
RangesOverlap (
  IN UINT64 StartA,
  IN UINT64 SizeA,
  IN UINT64 StartB,
  IN UINT64 SizeB
  )
{
  return (StartA < StartB + SizeB) && (StartB < StartA + SizeA);
}

STATIC
BOOLEAN
ValidateReservedMemory (
  IN CONST VOID *Fdt
  )
{
  CONST UINT32 *Reg;
  INT32         Length;
  INT32         MemoryNode;
  INT32         ReservedNode;
  INT32         ChildNode;
  UINT32        ReservedCount;
  UINT32        NoMapCount;
  UINT64        MemoryBase;
  UINT64        MemorySize;

  MemoryNode = FdtPathOffset (Fdt, "/memory");
  if (MemoryNode < 0) {
    return FALSE;
  }

  Reg = FdtGetProp (Fdt, MemoryNode, "reg", &Length);
  if ((Reg == NULL) || (Length != 4 * sizeof (UINT32))) {
    return FALSE;
  }

  MemoryBase = LShiftU64 (Fdt32ToCpu (Reg[0]), 32) | Fdt32ToCpu (Reg[1]);
  MemorySize = LShiftU64 (Fdt32ToCpu (Reg[2]), 32) | Fdt32ToCpu (Reg[3]);
  if ((MemoryBase != BARLEY_DRAM_BASE) || (MemorySize != BARLEY_DRAM_SIZE)) {
    return FALSE;
  }

  ReservedNode = FdtPathOffset (Fdt, "/reserved-memory");
  if (ReservedNode < 0) {
    return FALSE;
  }

  ReservedCount = 0;
  NoMapCount    = 0;
  FdtForEachSubnode (ChildNode, Fdt, ReservedNode) {
    UINT64 ReservedBase;
    UINT64 ReservedSize;

    Reg = FdtGetProp (Fdt, ChildNode, "reg", &Length);
    if (Reg == NULL) {
      continue;
    }

    if (Length != 4 * sizeof (UINT32)) {
      return FALSE;
    }

    ReservedBase = LShiftU64 (Fdt32ToCpu (Reg[0]), 32) | Fdt32ToCpu (Reg[1]);
    ReservedSize = LShiftU64 (Fdt32ToCpu (Reg[2]), 32) | Fdt32ToCpu (Reg[3]);
    ReservedCount++;

    if (FdtGetProp (Fdt, ChildNode, "no-map", NULL) != NULL) {
      NoMapCount++;
    }

    for (UINTN Index = 0; Index < ARRAY_SIZE (gBaseMemoryDescriptor); Index++) {
      if ((gBaseMemoryDescriptor[Index].ResourceType == SYS_MEM) &&
          RangesOverlap (
            ReservedBase,
            ReservedSize,
            gBaseMemoryDescriptor[Index].Address,
            gBaseMemoryDescriptor[Index].Length
            ))
      {
        DEBUG ((EFI_D_ERROR, "Reserved FDT range 0x%Lx+0x%Lx overlaps %a\n",
          ReservedBase, ReservedSize, gBaseMemoryDescriptor[Index].Name));
        return FALSE;
      }
    }

    for (UINTN Index = 0; Index < ARRAY_SIZE (gStableMblocks); Index++) {
      if (RangesOverlap (
            ReservedBase,
            ReservedSize,
            gStableMblocks[Index].Address,
            gStableMblocks[Index].Length
            ))
      {
        DEBUG ((EFI_D_ERROR, "Reserved FDT range 0x%Lx+0x%Lx overlaps %a\n",
          ReservedBase, ReservedSize, gStableMblocks[Index].Name));
        return FALSE;
      }
    }
  }

  DEBUG ((EFI_D_INFO, "Barley live FDT: %u reserved nodes, %u no-map\n",
    ReservedCount, NoMapCount));
  return (ReservedCount == BARLEY_RESERVED_NODE_COUNT) &&
         (NoMapCount == BARLEY_NO_MAP_NODE_COUNT);
}

STATIC
BOOLEAN
ValidateLiveFdt (
  VOID
  )
{
  CONST VOID *Fdt;
  UINT32      FdtSize;

  Fdt = (CONST VOID *)(UINTN)BARLEY_LIVE_FDT_BASE;
  if (FdtCheckHeader (Fdt) != 0) {
    DEBUG ((EFI_D_ERROR, "No valid Lenovo LK FDT at 0x%Lx\n", BARLEY_LIVE_FDT_BASE));
    return FALSE;
  }

  FdtSize = FdtTotalSize (Fdt);
  if ((FdtSize == 0) || (FdtSize > BARLEY_LIVE_FDT_MAX_SIZE)) {
    DEBUG ((EFI_D_ERROR, "Unexpected Lenovo LK FDT size: 0x%x\n", FdtSize));
    return FALSE;
  }

  return ValidateReservedMemory (Fdt);
}

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount
  )
{
  if (!gMemoryMapInitialized) {
    BOOLEAN LiveFdtValid;

    CopyMem (gMemoryDescriptor, gBaseMemoryDescriptor, sizeof (gBaseMemoryDescriptor));
    gMemoryDescriptorCount = ARRAY_SIZE (gBaseMemoryDescriptor);

    // Fail closed: add broad usable RAM only after the live LK FDT agrees with
    // both captures. The stack, FD, stable DXE heap, and MMIO remain available
    // for diagnostics if validation fails.
    BarleyEarlyVisualTrace (
      BARLEY_TRACE_STAGE_BEFORE_MEMORY_MAP,
      BARLEY_TRACE_BEFORE_MEMORY_MAP_A,
      BARLEY_TRACE_BEFORE_MEMORY_MAP_B
      );
    LiveFdtValid = ValidateLiveFdt ();
    BarleyEarlyVisualTrace (
      BARLEY_TRACE_STAGE_AFTER_MEMORY_MAP,
      BARLEY_TRACE_AFTER_MEMORY_MAP_A,
      BARLEY_TRACE_AFTER_MEMORY_MAP_B
      );

    if (LiveFdtValid) {
      CopyMem (
        &gMemoryDescriptor[gMemoryDescriptorCount],
        gStableMblocks,
        sizeof (gStableMblocks)
        );
      gMemoryDescriptorCount += ARRAY_SIZE (gStableMblocks);
    }

    gMemoryMapInitialized = TRUE;
  }

  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = gMemoryDescriptorCount;
}
