# Compile-only scaffold build driver (mirrors barleyPkg/DeviceBuild.py
# structure, pointed at UsbScaffoldTest/UsbScaffold.dsc).
import os
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_platform_build import Edk2PlatformBuild
from edk2toolext.invocables.edk2_setup import Edk2PlatformSetup, SetupSettingsManager, RequiredSubmodule
from edk2toolext.invocables.edk2_update import Edk2Update, UpdateSettingsManager
import logging

class CommonPlatform:
    PackagesSupported = ("UsbScaffold",)
    ArchSupported = ("AARCH64",)
    TargetsSupported = ("DEBUG", "RELEASE")
    Scopes = ('barley', 'gcc_aarch64_linux', 'edk2-build')
    WorkspaceRoot = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    PackagesPath = (
        "Platforms/Lenovo",
        "Common/Mu",
        "Common/Mu_OEM_Sample",
        "Mu_Basecore",
        "Silicon/MediaTek",
        "Silicon/Silicium",
        "Silicium-ACPI",
        "Silicium-ACPI/Platforms/Lenovo",
        "Silicium-ACPI/SoCs/MediaTek"
    )

class SettingsManager (UpdateSettingsManager, SetupSettingsManager, BuildSettingsManager):

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetPackagesPath (self):
        return list (CommonPlatform.PackagesPath)

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def GetName (self):
        return "UsbScaffold"

    def GetDependencies (self):
        return []

    def GetArchSupported (self):
        return CommonPlatform.ArchSupported

    def GetTargetsSupported (self):
        return CommonPlatform.TargetsSupported

    def GetRequiredSubmodules (self):
        return []

    def SetPlatformEnv (self):
        self.env.SetValue ("PRODUCT_NAME", "UsbScaffold", "Platform Hardcoded")
        self.env.SetValue ("ACTIVE_PLATFORM", "UsbScaffoldTest/UsbScaffold.dsc", "Platform Hardcoded")
        self.env.SetValue ("TARGET_ARCH", "AARCH64", "Platform Hardcoded")
        self.env.SetValue ("TOOL_CHAIN_TAG", "CLANGPDB", "set default to clangpdb")
        self.env.SetValue ("BUILDREPORTING", "FALSE", "disable")
        return 0

    def PlatformPreBuild (self):
        return 0

    def PlatformPostBuild (self):
        return 0

    def FlashRomImage (self):
        return 0

if __name__ == "__main__":
    import argparse, sys
    parser = argparse.ArgumentParser (add_help=False)
    parse_group = parser.add_mutually_exclusive_group()
    parse_group.add_argument ("--update", "--UPDATE", action='store_true', help="Invokes stuart_update")
    parse_group.add_argument ("--setup", "--SETUP", action='store_true', help="Invokes stuart_setup")
    args, remaining = parser.parse_known_args()
    remaining.append("--verbose")
    if args.update:
        Edk2Update().Invoke()
    elif args.setup:
        Edk2PlatformSetup().Invoke()
    else:
        Edk2PlatformBuild().Invoke()
