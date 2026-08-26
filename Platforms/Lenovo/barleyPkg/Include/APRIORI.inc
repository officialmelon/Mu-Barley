APRIORI DXE {
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf
  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  INF EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf

  # Match the proven MT6768 storage ordering used by lancelot.  Providers are
  # available before BDS, so the normal Silicium boot policy can connect media
  # without a platform-specific dispatcher hook.
  INF MediaTekPkg/Drivers/GpioDxe/GpioDxe.inf
  INF MediaTekPkg/Drivers/ClockDxe/ClockDxe.inf
  INF MediaTekPkg/Drivers/PmicWrapperDxe/PmicWrapperDxe.inf
  INF MediaTekPkg/Drivers/MT6358PmicDxe/MT6358PmicDxe.inf
  INF barleyPkg/Drivers/BarleyTouchPowerDxe/BarleyTouchPowerDxe.inf
  INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  INF MediaTekPkg/Drivers/MsdcDxe/MsdcDxe.inf
  INF MdeModulePkg/Bus/Sd/EmmcDxe/EmmcDxe.inf
  INF MdeModulePkg/Bus/Sd/SdDxe/SdDxe.inf
  INF FatPkg/EnhancedFatDxe/Fat.inf
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf

  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  INF EmbeddedPkg/EmbeddedMonotonicCounter/EmbeddedMonotonicCounter.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF MdeModulePkg/Universal/PrintDxe/PrintDxe.inf
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  # The PMIC provider above must exist before the platform keypad library
  # constructor is invoked.
  INF SiliciumPkg/Drivers/KeypadDeviceDxe/KeypadDeviceDxe.inf
  INF SiliciumPkg/Drivers/KeypadDxe/KeypadDxe.inf
  INF barleyPkg/Drivers/BarleyLkGopDxe/BarleyLkGopDxe.inf
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf
}
