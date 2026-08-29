##
# Compile-only verification platform for the MediaTek USB scaffolding.
# This DSC is NOT part of any bootable image: it builds the new USB
# libraries/drivers against the standard platform dependency graph so the
# scaffold can be type- and link-checked without touching barley.dsc.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = UsbScaffold
  PLATFORM_GUID                  = 7E2B7B4C-93C1-4F7E-8B0A-1D6E4A2C55A1
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/UsbScaffold
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  ENABLE_SECUREBOOT              = 0

!include MT6768Pkg/MT6768Pkg.dsc.inc

[PcdsFixedAtBuild]
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000

[LibraryClasses]
  MtkMusbCoreLib|MediaTekPkg/Library/MtkMusbCoreLib/MtkMusbCoreLib.inf
  MtkUsbPhyLib|MediaTekPkg/Library/MtkUsbPhyLib/MtkUsbPhyLib.inf
  BarleyUsbPortLib|barleyPkg/Library/BarleyUsbPortLib/BarleyUsbPortLib.inf
  MemoryMapLib|barleyPkg/Library/MemoryMapLib/MemoryMapLib.inf
  PlatformSecLib|barleyPkg/Library/PlatformSecLib/PlatformSecLib.inf
  DeviceBootManagerLib|barleyPkg/Library/DeviceBootManagerLib/DeviceBootManagerLib.inf
  KeypadDeviceLib|barleyPkg/Library/KeypadDeviceLib/KeypadDeviceLib.inf

[Components]
  MediaTekPkg/Library/MtkMusbCoreLib/MtkMusbCoreLib.inf
  MediaTekPkg/Library/MtkUsbPhyLib/MtkUsbPhyLib.inf
  barleyPkg/Library/BarleyUsbPortLib/BarleyUsbPortLib.inf
  MediaTekPkg/Drivers/MtkMusbHostDxe/MtkMusbHostDxe.inf
  MediaTekPkg/Drivers/MtkMusbDeviceDxe/MtkMusbDeviceDxe.inf
