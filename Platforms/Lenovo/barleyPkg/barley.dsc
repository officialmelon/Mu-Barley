##
# Lenovo Tab M11 (TB330XU / barley_row_lte) platform definition.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = barley
  PLATFORM_GUID                  = 95F2F93C-1B5B-4DA4-A20B-D7440A85D9A7
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/barleyPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = barleyPkg/barley.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 1

  # MT6769H / MT8786 (Helio G88), using the MT6768 BSP architecture.
  SOC_TYPE                       = 2

!include MT6768Pkg/MT6768Pkg.dsc.inc

[BuildOptions]
  *_CLANGPDB_AARCH64_CC_FLAGS = -D BARLEY_STAGE_TRACE=1
  *_CLANGPDB_AARCH64_PP_FLAGS = -D BARLEY_STAGE_TRACE=1

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000

  #
  # UEFI Stack
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x40000000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Lenovo"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Lenovo Tab M11"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"TB330XU"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"barley_row_lte"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"Barley"

  #
  # Inherited LK frame buffer. Pixels-per-scanline remains provisional.
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1200
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|1920
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Storage
  #
  gMediaTekPkgTokenSpaceGuid.PcdStorageIsEMMC|TRUE

[LibraryClasses]
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  MemoryMapLib|barleyPkg/Library/MemoryMapLib/MemoryMapLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf

[Components]
  barleyPkg/Drivers/EarlyVisualTraceDxe/EarlyVisualTraceDxe.inf
  barleyPkg/Drivers/BarleyLkGopDxe/BarleyLkGopDxe.inf

  ShellPkg/Application/Shell/Shell.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
    <LibraryClasses>
      NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
  }
