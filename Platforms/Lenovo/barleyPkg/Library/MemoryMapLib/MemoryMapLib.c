#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryMapLib.h>

#define BARLEY_DRAM_BASE                 0x40000000ULL
#define BARLEY_DRAM_SIZE                 0x200000000ULL
#define BARLEY_DRAM_END                  (BARLEY_DRAM_BASE + BARLEY_DRAM_SIZE)
#define BARLEY_FALLBACK_FDT_BASE         0x4BC80000ULL
#define BARLEY_FDT_MAX_SIZE              0x00080000U
#define BARLEY_LK_HANDOFF_BASE           0x40040000ULL
#define BARLEY_LK_HANDOFF_SIZE           0x00001000ULL
#define BARLEY_LK_HANDOFF_MAGIC          0x2130585444464B4CULL
#define BARLEY_PAGE_SIZE                 0x00001000ULL
#define BARLEY_MBLOCK_ENTRY_SIZE         24U
#define BARLEY_MAX_MBLOCKS               128U
#define BARLEY_MAX_BLOCKED_RANGES        40U
#define BARLEY_MAX_MEMORY_DESCRIPTORS    \
  (MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT - 1U)

#define BARLEY_FRAMEBUFFER_BASE          0x7BCE0000ULL
#define BARLEY_FRAMEBUFFER_SIZE          0x01F20000ULL
#define BARLEY_DISPLAY_AUX_BASE          0x7E605000ULL
#define BARLEY_DISPLAY_AUX_SIZE          0x017E8000ULL

typedef struct {
  UINT64    Magic;
  UINT64    Fdt;
} BARLEY_LK_HANDOFF;

typedef struct {
  UINT64    Start;
  UINT64    Size;
  UINT32    Rank;
  UINT32    Flags;
} BARLEY_LK_MBLOCK;

typedef struct {
  UINT64    Start;
  UINT64    End;
} BARLEY_BLOCKED_RANGE;

STATIC_ASSERT (
  sizeof (BARLEY_LK_MBLOCK) == BARLEY_MBLOCK_ENTRY_SIZE,
  "MediaTek LK mblock layout must remain 24 bytes"
  );

STATIC CONST EFI_MEMORY_REGION_DESCRIPTOR  gFixedMemoryDescriptor[] = {
  // Name, Address, Length, HobOption, ResourceType, ResourceAttribute, MemoryType, ArmAttribute
  {"UEFI Stack",         0x40000000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  // The first SEC instruction saves LK's x0 FDT handoff here.  This page sits
  // between the verified stack mblock and LK's kernel load address.
  {"LK Handoff",         BARLEY_LK_HANDOFF_BASE, BARLEY_LK_HANDOFF_SIZE, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  // BootShim relocates the complete FD here before entering SEC.
  {"UEFI FD",            0x4BD00000, 0x00200000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  // LK mblock 7 is stable across both independent captures and supplies the
  // named SEC/DXE HOB heap expected by the common Silicium SEC implementation.
  {"DXE Heap",           0x56000000, 0x16000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK},
  // LK's three scanout surfaces live in this fixed no-map carveout.  UEFI maps
  // only this explicitly consumed no-map region so the passive GOP can draw.
  {"LK Framebuffer",     BARLEY_FRAMEBUFFER_BASE, BARLEY_FRAMEBUFFER_SIZE, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH},

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
  {"MSDC Top-0",         0x11CD0000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC Top-1",         0x11C90000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display MMSYS",      0x14000000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display OVL0",       0x1400B000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display OVL0 2L",    0x1400C000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display RDMA0",      0x1400D000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display DSI0",       0x14014000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}
};

// Safe fallback only.  These free mblocks were byte-identical in both live LK
// FDT captures.  They are used if neither the x0 FDT nor the historical Lenovo
// address contains a parseable mblock list.  The fallback never exposes DRAM
// wholesale and deliberately omits mblock 11, which overlaps Display Aux.
STATIC CONST EFI_MEMORY_REGION_DESCRIPTOR  gFallbackMblocks[] = {
  {"LK Mblock 02", 0x4BF00000, 0x00180000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 03", 0x4C280000, 0x00180000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 04", 0x4C800000, 0x00600000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 05", 0x4CE60000, 0x001A0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 06", 0x4D100000, 0x00F00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 08", 0x6C900000, 0x03700000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 09", 0x73C00000, 0x043F0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
  {"LK Mblock 10", 0x79600000, 0x00DF8000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
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

STATIC EFI_MEMORY_REGION_DESCRIPTOR  gMemoryDescriptor[MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT];
STATIC BARLEY_BLOCKED_RANGE           gBlockedRange[BARLEY_MAX_BLOCKED_RANGES];
STATIC UINT8                          gMemoryDescriptorCount;
STATIC UINTN                          gBlockedRangeCount;
STATIC BOOLEAN                        gMemoryMapInitialized;

STATIC
BOOLEAN
GetRangeEnd (
  IN  UINT64  Start,
  IN  UINT64  Size,
  OUT UINT64  *End
  )
{
  if ((Size == 0) || (Start > MAX_UINT64 - Size)) {
    return FALSE;
  }

  *End = Start + Size;
  return TRUE;
}

STATIC
BOOLEAN
GetPageAlignedRange (
  IN  UINT64  Start,
  IN  UINT64  Size,
  OUT UINT64  *AlignedStart,
  OUT UINT64  *AlignedSize
  )
{
  UINT64  End;

  if (!GetRangeEnd (Start, Size, &End) ||
      (End > MAX_UINT64 - (BARLEY_PAGE_SIZE - 1U)))
  {
    return FALSE;
  }

  *AlignedStart = Start & ~(BARLEY_PAGE_SIZE - 1U);
  End           = (End + BARLEY_PAGE_SIZE - 1U) & ~(BARLEY_PAGE_SIZE - 1U);
  if ((*AlignedStart < BARLEY_DRAM_BASE) || (End > BARLEY_DRAM_END)) {
    return FALSE;
  }

  *AlignedSize = End - *AlignedStart;
  return TRUE;
}

STATIC
BOOLEAN
RangesOverlap (
  IN UINT64  StartA,
  IN UINT64  EndA,
  IN UINT64  StartB,
  IN UINT64  EndB
  )
{
  return (StartA < EndB) && (StartB < EndA);
}

STATIC
BOOLEAN
AppendDescriptor (
  IN CONST CHAR8                   *Name,
  IN UINTN                         NameLength,
  IN EFI_PHYSICAL_ADDRESS          Address,
  IN UINT64                        Length,
  IN EFI_MEMORY_HOB_OPTION         HobOption,
  IN EFI_RESOURCE_TYPE             ResourceType,
  IN EFI_RESOURCE_ATTRIBUTE_TYPE   ResourceAttribute,
  IN EFI_MEMORY_TYPE               MemoryType,
  IN ARM_MEMORY_REGION_ATTRIBUTES  ArmAttributes
  )
{
  EFI_MEMORY_REGION_DESCRIPTOR  *Descriptor;

  if ((Length == 0) ||
      (gMemoryDescriptorCount >= BARLEY_MAX_MEMORY_DESCRIPTORS))
  {
    return FALSE;
  }

  Descriptor = &gMemoryDescriptor[gMemoryDescriptorCount++];
  ZeroMem (Descriptor, sizeof (*Descriptor));
  if (NameLength >= sizeof (Descriptor->Name)) {
    NameLength = sizeof (Descriptor->Name) - 1U;
  }

  CopyMem (Descriptor->Name, Name, NameLength);
  Descriptor->Address           = Address;
  Descriptor->Length            = Length;
  Descriptor->HobOption         = HobOption;
  Descriptor->ResourceType      = ResourceType;
  Descriptor->ResourceAttribute = ResourceAttribute;
  Descriptor->MemoryType        = MemoryType;
  Descriptor->ArmAttributes     = ArmAttributes;
  return TRUE;
}

#define APPEND_DESCRIPTOR(Name, Address, Length, Hob, Resource, Capability, Type, Attribute) \
  AppendDescriptor (Name, sizeof (Name) - 1U, Address, Length, Hob, Resource, Capability, Type, Attribute)

STATIC
VOID
ResetMemoryMap (
  VOID
  )
{
  ZeroMem (gMemoryDescriptor, sizeof (gMemoryDescriptor));
  CopyMem (
    gMemoryDescriptor,
    gFixedMemoryDescriptor,
    sizeof (gFixedMemoryDescriptor)
    );
  gMemoryDescriptorCount = ARRAY_SIZE (gFixedMemoryDescriptor);
  gBlockedRangeCount      = 0;
}

STATIC
BOOLEAN
AddBlockedRange (
  IN UINT64  Start,
  IN UINT64  Size
  )
{
  UINT64  AlignedSize;
  UINT64  AlignedStart;

  if (!GetPageAlignedRange (Start, Size, &AlignedStart, &AlignedSize) ||
      (gBlockedRangeCount >= BARLEY_MAX_BLOCKED_RANGES))
  {
    return FALSE;
  }

  gBlockedRange[gBlockedRangeCount].Start = AlignedStart;
  gBlockedRange[gBlockedRangeCount].End   = AlignedStart + AlignedSize;
  gBlockedRangeCount++;
  return TRUE;
}

STATIC
CONST VOID *
ValidateFdtAddress (
  IN UINT64  Address
  )
{
  CONST VOID  *Fdt;
  UINT32      FdtSize;

  if (((Address & (sizeof (UINT64) - 1U)) != 0) ||
      (Address < BARLEY_DRAM_BASE) ||
      (Address > BARLEY_DRAM_END - sizeof (FDT_HEADER)))
  {
    return NULL;
  }

  Fdt = (CONST VOID *)(UINTN)Address;
  if (FdtCheckHeader (Fdt) != 0) {
    return NULL;
  }

  FdtSize = FdtTotalSize (Fdt);
  if ((FdtSize < sizeof (FDT_HEADER)) ||
      (FdtSize > BARLEY_FDT_MAX_SIZE) ||
      (Address > BARLEY_DRAM_END - FdtSize))
  {
    return NULL;
  }

  return Fdt;
}

STATIC
CONST VOID *
FindLiveFdt (
  VOID
  )
{
  CONST BARLEY_LK_HANDOFF  *Handoff;
  CONST VOID               *Fdt;

  Handoff = (CONST BARLEY_LK_HANDOFF *)(UINTN)BARLEY_LK_HANDOFF_BASE;
  if (Handoff->Magic == BARLEY_LK_HANDOFF_MAGIC) {
    Fdt = ValidateFdtAddress (Handoff->Fdt);
    if (Fdt != NULL) {
      DEBUG ((EFI_D_INFO, "Barley LK FDT from x0: 0x%Lx\n", Handoff->Fdt));
      return Fdt;
    }

    DEBUG ((EFI_D_ERROR, "Invalid Barley LK x0 FDT: 0x%Lx\n", Handoff->Fdt));
  }

  Fdt = ValidateFdtAddress (BARLEY_FALLBACK_FDT_BASE);
  if (Fdt != NULL) {
    DEBUG ((EFI_D_WARN, "Using historical Barley FDT address: 0x%Lx\n", BARLEY_FALLBACK_FDT_BASE));
  }

  return Fdt;
}

STATIC
BOOLEAN
ReadDramRange (
  IN  CONST VOID  *Fdt,
  OUT INT32       *MemoryNode
  )
{
  CONST UINT32  *Reg;
  INT32         Length;
  UINT64        MemoryBase;
  UINT64        MemorySize;

  *MemoryNode = FdtPathOffset (Fdt, "/memory");
  if (*MemoryNode < 0) {
    return FALSE;
  }

  Reg = FdtGetProp (Fdt, *MemoryNode, "reg", &Length);
  if ((Reg == NULL) || (Length != 4 * sizeof (UINT32))) {
    return FALSE;
  }

  MemoryBase = LShiftU64 (Fdt32ToCpu (Reg[0]), 32) | Fdt32ToCpu (Reg[1]);
  MemorySize = LShiftU64 (Fdt32ToCpu (Reg[2]), 32) | Fdt32ToCpu (Reg[3]);
  return (MemoryBase == BARLEY_DRAM_BASE) && (MemorySize == BARLEY_DRAM_SIZE);
}

STATIC
BOOLEAN
AppendReservedMemory (
  IN CONST VOID  *Fdt,
  OUT UINT32     *ReservedCount,
  OUT UINT32     *NoMapCount
  )
{
  INT32  ReservedNode;
  INT32  ChildNode;

  ReservedNode = FdtPathOffset (Fdt, "/reserved-memory");
  if (ReservedNode < 0) {
    return FALSE;
  }

  *ReservedCount = 0;
  *NoMapCount    = 0;
  FdtForEachSubnode (ChildNode, Fdt, ReservedNode) {
    CONST UINT32  *Reg;
    INT32         Length;
    BOOLEAN       NoMap;

    Reg = FdtGetProp (Fdt, ChildNode, "reg", &Length);
    if (Reg == NULL) {
      continue;
    }

    if ((Length <= 0) || ((Length % (4 * sizeof (UINT32))) != 0)) {
      return FALSE;
    }

    NoMap = FdtGetProp (Fdt, ChildNode, "no-map", NULL) != NULL;
    for (INT32 Offset = 0; Offset < Length / (INT32)sizeof (UINT32); Offset += 4) {
      UINT64  AlignedBase;
      UINT64  AlignedSize;
      UINT64  ReservedBase;
      UINT64  ReservedSize;
      UINT64  ReservedEnd;

      ReservedBase = LShiftU64 (Fdt32ToCpu (Reg[Offset]), 32) |
                     Fdt32ToCpu (Reg[Offset + 1]);
      ReservedSize = LShiftU64 (Fdt32ToCpu (Reg[Offset + 2]), 32) |
                     Fdt32ToCpu (Reg[Offset + 3]);
      if (!GetRangeEnd (ReservedBase, ReservedSize, &ReservedEnd) ||
          (ReservedBase < BARLEY_DRAM_BASE) ||
          (ReservedEnd > BARLEY_DRAM_END) ||
          !GetPageAlignedRange (
             ReservedBase,
             ReservedSize,
             &AlignedBase,
             &AlignedSize
             ) ||
          !AddBlockedRange (ReservedBase, ReservedSize))
      {
        return FALSE;
      }

      (*ReservedCount)++;
      if (NoMap) {
        (*NoMapCount)++;
      }

      // The fixed framebuffer descriptor above is the one intentional no-map
      // exception: the passive GOP must map this exact LK scanout carveout.
      if ((ReservedBase == BARLEY_FRAMEBUFFER_BASE) &&
          (ReservedSize == BARLEY_FRAMEBUFFER_SIZE))
      {
        continue;
      }

      if (NoMap) {
        if (!APPEND_DESCRIPTOR (
               "FDT no-map",
               AlignedBase,
               AlignedSize,
               HobOnlyNoCacheSetting,
               MEM_RES,
               SYS_MEM_CAP,
               Reserv,
               WRITE_BACK
               ))
        {
          return FALSE;
        }
      } else if (!APPEND_DESCRIPTOR (
                    "FDT reserved",
                    AlignedBase,
                    AlignedSize,
                    AddMem,
                    MEM_RES,
                    SYS_MEM_CAP,
                    Reserv,
                    WRITE_BACK
                    ))
      {
        return FALSE;
      }
    }
  }

  return *ReservedCount != 0;
}

STATIC
BOOLEAN
AppendMblockFragment (
  IN UINT64      Start,
  IN UINT64      End,
  IN OUT UINT64  *UsableBytes
  )
{
  if (Start >= End) {
    return TRUE;
  }

  if (!APPEND_DESCRIPTOR (
         "LK Mblock",
         Start,
         End - Start,
         AddMem,
         SYS_MEM,
         SYS_MEM_CAP,
         Conv,
         WRITE_BACK
         ))
  {
    return FALSE;
  }

  *UsableBytes += End - Start;
  return TRUE;
}

STATIC
BOOLEAN
AppendUsableMblock (
  IN UINT64      MblockStart,
  IN UINT64      MblockSize,
  IN OUT UINT64  *UsableBytes
  )
{
  UINT64  Cursor;
  UINT64  MblockEnd;

  if (!GetRangeEnd (MblockStart, MblockSize, &MblockEnd)) {
    return FALSE;
  }

  Cursor = MblockStart;
  while (Cursor < MblockEnd) {
    UINT64  BlockEnd;
    UINT64  BlockStart;
    BOOLEAN Found;

    BlockStart = MblockEnd;
    BlockEnd   = MblockEnd;
    Found      = FALSE;
    for (UINTN Index = 0; Index < gBlockedRangeCount; Index++) {
      if (!RangesOverlap (
             Cursor,
             MblockEnd,
             gBlockedRange[Index].Start,
             gBlockedRange[Index].End
             ))
      {
        continue;
      }

      if (!Found || (gBlockedRange[Index].Start < BlockStart)) {
        BlockStart = gBlockedRange[Index].Start;
        BlockEnd   = gBlockedRange[Index].End;
        Found      = TRUE;
      } else if ((gBlockedRange[Index].Start == BlockStart) &&
                 (gBlockedRange[Index].End > BlockEnd))
      {
        BlockEnd = gBlockedRange[Index].End;
      }
    }

    if (!Found) {
      return AppendMblockFragment (Cursor, MblockEnd, UsableBytes);
    }

    if ((BlockStart > Cursor) &&
        !AppendMblockFragment (Cursor, BlockStart, UsableBytes))
    {
      return FALSE;
    }

    if (BlockEnd <= Cursor) {
      return FALSE;
    }

    Cursor = BlockEnd;
  }

  return TRUE;
}

STATIC
BOOLEAN
AppendMblocks (
  IN CONST VOID  *Fdt,
  IN INT32       MemoryNode,
  OUT UINT32     *MblockCount,
  OUT UINT64     *UsableBytes
  )
{
  CONST UINT8       *Property;
  BARLEY_LK_MBLOCK  Mblock;
  INT32             Length;
  UINT32            Count;
  UINT64            PreviousEnd;

  Property = FdtGetProp (Fdt, MemoryNode, "mblock_info", &Length);
  if ((Property == NULL) || (Length < 8)) {
    return FALSE;
  }

  CopyMem (&Count, Property, sizeof (Count));
  if ((Count == 0) || (Count > BARLEY_MAX_MBLOCKS) ||
      ((UINT32)Length < 8U + (Count * BARLEY_MBLOCK_ENTRY_SIZE)))
  {
    return FALSE;
  }

  PreviousEnd = BARLEY_DRAM_BASE;
  *UsableBytes = 0;
  for (UINT32 Index = 0; Index < Count; Index++) {
    UINT64  End;

    CopyMem (
      &Mblock,
      Property + 8U + (Index * BARLEY_MBLOCK_ENTRY_SIZE),
      sizeof (Mblock)
      );
    if (Mblock.Size == 0) {
      continue;
    }

    if (!GetRangeEnd (Mblock.Start, Mblock.Size, &End) ||
        ((Mblock.Start & (BARLEY_PAGE_SIZE - 1U)) != 0) ||
        ((Mblock.Size & (BARLEY_PAGE_SIZE - 1U)) != 0) ||
        (Mblock.Start < BARLEY_DRAM_BASE) ||
        (End > BARLEY_DRAM_END) ||
        (Mblock.Start < PreviousEnd))
    {
      return FALSE;
    }

    PreviousEnd = End;
    if (!AppendUsableMblock (Mblock.Start, Mblock.Size, UsableBytes)) {
      return FALSE;
    }
  }

  *MblockCount = Count;
  return TRUE;
}

STATIC
BOOLEAN
BuildDynamicMemoryMap (
  IN CONST VOID  *Fdt
  )
{
  INT32   MemoryNode;
  UINT32  FdtSize;
  UINT32  MblockCount;
  UINT32  NoMapCount;
  UINT32  ReservedCount;
  UINT64  FdtAlignedBase;
  UINT64  FdtAlignedSize;
  UINT64  UsableBytes;

  if (!ReadDramRange (Fdt, &MemoryNode)) {
    return FALSE;
  }

  // Every fixed RAM descriptor is unavailable to the dynamic free-mblock
  // list.  MMIO descriptors sit below DRAM and are ignored here.
  for (UINTN Index = 0; Index < ARRAY_SIZE (gFixedMemoryDescriptor); Index++) {
    if ((gFixedMemoryDescriptor[Index].Address >= BARLEY_DRAM_BASE) &&
        !AddBlockedRange (
           gFixedMemoryDescriptor[Index].Address,
           gFixedMemoryDescriptor[Index].Length
           ))
    {
      return FALSE;
    }
  }

  // This separate display allocation is not the OVL framebuffer.  It is not
  // mapped or consumed by UEFI, but it must never become conventional RAM.
  if (!AddBlockedRange (BARLEY_DISPLAY_AUX_BASE, BARLEY_DISPLAY_AUX_SIZE) ||
      !AppendReservedMemory (Fdt, &ReservedCount, &NoMapCount))
  {
    return FALSE;
  }

  FdtSize = FdtTotalSize (Fdt);
  if (!GetPageAlignedRange (
         (UINT64)(UINTN)Fdt,
         FdtSize,
         &FdtAlignedBase,
         &FdtAlignedSize
         ) ||
      !AddBlockedRange ((UINT64)(UINTN)Fdt, FdtSize) ||
      !APPEND_DESCRIPTOR (
         "Live LK FDT",
         FdtAlignedBase,
         FdtAlignedSize,
         AddMem,
         MEM_RES,
         SYS_MEM_CAP,
         Reserv,
         WRITE_BACK
         ) ||
      !AppendMblocks (Fdt, MemoryNode, &MblockCount, &UsableBytes))
  {
    return FALSE;
  }

  DEBUG ((
    EFI_D_INFO,
    "Barley dynamic memory: %u mblocks, %u reserved (%u no-map), %Lu usable bytes\n",
    MblockCount,
    ReservedCount,
    NoMapCount,
    UsableBytes
    ));
  return TRUE;
}

STATIC
VOID
BuildFallbackMemoryMap (
  IN CONST VOID  *Fdt OPTIONAL
  )
{
  UINT64  FdtEnd;
  UINT64  FdtStart;
  UINT64  FdtSizeAligned;
  UINT32  FdtSize;

  FdtStart = 0;
  FdtEnd   = 0;
  if (Fdt != NULL) {
    FdtSize = FdtTotalSize (Fdt);
    if (GetPageAlignedRange (
          (UINT64)(UINTN)Fdt,
          FdtSize,
          &FdtStart,
          &FdtSizeAligned
          ))
    {
      FdtEnd = FdtStart + FdtSizeAligned;
      APPEND_DESCRIPTOR (
        "Live LK FDT",
        FdtStart,
        FdtSizeAligned,
        AddMem,
        MEM_RES,
        SYS_MEM_CAP,
        Reserv,
        WRITE_BACK
        );
    }
  }

  for (UINTN Index = 0; Index < ARRAY_SIZE (gFallbackMblocks); Index++) {
    UINT64  End;

    if (!GetRangeEnd (
           gFallbackMblocks[Index].Address,
           gFallbackMblocks[Index].Length,
           &End
           ))
    {
      continue;
    }

    // Fail closed if a moved but otherwise valid FDT occupies a fallback
    // mblock.  Losing one region is safer than exposing the live DTB as RAM.
    if ((Fdt != NULL) &&
        RangesOverlap (
          gFallbackMblocks[Index].Address,
          End,
          FdtStart,
          FdtEnd
          ))
    {
      continue;
    }

    if (gMemoryDescriptorCount >= BARLEY_MAX_MEMORY_DESCRIPTORS) {
      break;
    }

    CopyMem (
      &gMemoryDescriptor[gMemoryDescriptorCount++],
      &gFallbackMblocks[Index],
      sizeof (gFallbackMblocks[Index])
      );
  }

  DEBUG ((EFI_D_ERROR, "Using conservative Barley fallback mblock map\n"));
}

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR  **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount
  )
{
  if (!gMemoryMapInitialized) {
    CONST VOID  *Fdt;

    ResetMemoryMap ();
    Fdt = FindLiveFdt ();
    if ((Fdt == NULL) || !BuildDynamicMemoryMap (Fdt)) {
      ResetMemoryMap ();
      BuildFallbackMemoryMap (Fdt);
    }

    gMemoryMapInitialized = TRUE;
  }

  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = gMemoryDescriptorCount;
}
