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

[PcdsFixedAtBuild]
  # Barley does not include the HII FormBrowser2/DriverHealth UI.  The generic
  # UefiBootManagerLib default points at that missing formset and asserts at
  # ReadyToBoot before it can start the FV-resident Shell or an OS loader.
  # A zero GUID is the documented way to disable that optional repair UI.
  gEfiMdeModulePkgTokenSpaceGuid.PcdDriverHealthConfigureForm|{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

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
  # Host 0 is soldered eMMC; host 1 is the removable microSD slot described by
  # both independently captured Lenovo LK-patched FDTs.
  gMediaTekPkgTokenSpaceGuid.PcdMsdcHostMask|0x3
  # LK has already established the eMMC supply and pinmux.  Preserve that
  # verified working state while the common controller driver takes over.
  gMediaTekPkgTokenSpaceGuid.PcdMsdcPreserveBootStateMask|0x1
  # Lenovo wires the slot's VMMC request through MT6358 VMCH_EINT_LOW and its
  # VQMMC rail through VMC.  GPIO18 is an active-high card-detect input.
  gMediaTekPkgTokenSpaceGuid.PcdMsdcRemovableVmmcRegulatorName|"ldo_vmch_eint_low"
  gMediaTekPkgTokenSpaceGuid.PcdMsdcRemovableVqmmcRegulatorName|"ldo_vmc"
  gMediaTekPkgTokenSpaceGuid.PcdMsdcRemovableCardDetectPin|18
  gMediaTekPkgTokenSpaceGuid.PcdMsdcRemovableCardDetectActiveHigh|TRUE
  # Lenovo routes all eight eMMC data lines and all four microSD data lines.
  # MsdcDxe negotiates each width with the card before changing the host.
  gMediaTekPkgTokenSpaceGuid.PcdMsdcEmmcBusWidth|8
  gMediaTekPkgTokenSpaceGuid.PcdMsdcRemovableBusWidth|4
  # Start at a conservative SD default-speed clock.  High-speed/UHS tuning is
  # deliberately deferred until basic removable Block I/O is proven.
  gMediaTekPkgTokenSpaceGuid.PcdMsdcRemovableMaxClockHz|25000000

[LibraryClasses]
!if $(TARGET) == DEBUG
  # Lenovo LK uses three differently pitched scanout surfaces.
  SerialPortLib|barleyPkg/Library/BarleyLkFrameBufferSerialPortLib/BarleyLkFrameBufferSerialPortLib.inf
!endif
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf
  MemoryMapLib|barleyPkg/Library/MemoryMapLib/MemoryMapLib.inf
  PlatformSecLib|barleyPkg/Library/PlatformSecLib/PlatformSecLib.inf
  DeviceBootManagerLib|barleyPkg/Library/DeviceBootManagerLib/DeviceBootManagerLib.inf
  KeypadDeviceLib|barleyPkg/Library/KeypadDeviceLib/KeypadDeviceLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf

[Components]
  barleyPkg/Drivers/BarleyLkGopDxe/BarleyLkGopDxe.inf
  barleyPkg/Drivers/BarleyTouchPowerDxe/BarleyTouchPowerDxe.inf
  SiliciumPkg/Drivers/KeypadDeviceDxe/KeypadDeviceDxe.inf
  SiliciumPkg/Drivers/KeypadDxe/KeypadDxe.inf

  ShellPkg/Application/Shell/Shell.inf {
    <PcdsFixedAtBuild>
      gEfiShellPkgTokenSpaceGuid.PcdShellLibAutoInitialize|FALSE
    <LibraryClasses>
      NULL|ShellPkg/Library/UefiShellLevel2CommandsLib/UefiShellLevel2CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel1CommandsLib/UefiShellLevel1CommandsLib.inf
      NULL|ShellPkg/Library/UefiShellLevel3CommandsLib/UefiShellLevel3CommandsLib.inf
  }
