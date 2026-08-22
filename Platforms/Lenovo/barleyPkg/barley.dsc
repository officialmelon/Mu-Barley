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
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  # MT6769H / MT8786 (Helio G88), using the MT6768 BSP architecture.
  SOC_TYPE                       = 2

!include MT6768Pkg/MT6768Pkg.dsc.inc

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
  MemoryMapLib|barleyPkg/Library/MemoryMapLib/MemoryMapLib.inf
