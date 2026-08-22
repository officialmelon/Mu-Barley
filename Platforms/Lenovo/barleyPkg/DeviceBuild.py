##
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os

from edk2toolext.environment import shell_environment
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_pr_eval import PrEvalSettingsManager
from edk2toolext.invocables.edk2_setup import RequiredSubmodule, SetupSettingsManager
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toolext.invocables.edk2_parse import ParseSettingsManager


class CommonPlatform ():
    PackagesSupported = ("barleyPkg",)
    ArchSupported = ("AARCH64",)
    TargetsSupported = ("DEBUG", "RELEASE")
    Scopes = ('barley', 'gcc_aarch64_linux', 'edk2-build')
    WorkspaceRoot = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
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


class SettingsManager (UpdateSettingsManager, SetupSettingsManager, PrEvalSettingsManager, ParseSettingsManager):

    def GetPackagesSupported (self):
        return CommonPlatform.PackagesSupported

    def GetArchitecturesSupported (self):
        return CommonPlatform.ArchSupported

    def GetTargetsSupported (self):
        return CommonPlatform.TargetsSupported

    def GetRequiredSubmodules (self):
        return [
            RequiredSubmodule ("Binaries", True),
            RequiredSubmodule ("Common/Mu", True),
            RequiredSubmodule ("Common/Mu_OEM_Sample", True),
            RequiredSubmodule ("Mu_Basecore", True),
            RequiredSubmodule ("Silicium-ACPI", True),
            RequiredSubmodule ("Silicon/Silicium/OpensslPkg/Library/OpensslLib/openssl", True)
        ]

    def SetArchitectures (self, list_of_requested_architectures):
        unsupported = set(list_of_requested_architectures) - set(self.GetArchitecturesSupported())

        if len(unsupported) > 0:
            error_string = "Unsupported Architecture Requested: " + " ".join(unsupported)
            logging.critical (error_string)
            raise Exception (error_string)

        self.ActualArchitectures = list_of_requested_architectures

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def FilterPackagesToTest (self, changedFilesList: list, potentialPackagesList: list) -> list:
        build_these_packages = []
        possible_packages = potentialPackagesList.copy ()

        for file_name in changedFilesList:
            if "BaseTools" in file_name:
                if os.path.splitext(file_name) not in [".txt", ".md"]:
                    build_these_packages = possible_packages
                    break

            if "platform-build-run-steps.yml" in file_name:
                build_these_packages = possible_packages
                break

        return build_these_packages

    def GetPlatformDscAndConfig (self) -> tuple:
        return ("barleyPkg/barley.dsc", {})

    def GetName (self):
        return "barley"

    def GetPackagesPath (self):
        return CommonPlatform.PackagesPath


class PlatformBuilder (UefiBuilder, BuildSettingsManager):
    def __init__ (self):
        UefiBuilder.__init__ (self)

    def AddCommandLineOptions (self, parserObj):
        parserObj.add_argument('-a', "--arch", dest="build_arch", type=str, default="AARCH64", help="Optional - CSV of architecture to build. AARCH64 is the only valid option for this platform.")

    def RetrieveCommandLineOptions (self, args):
        if args.build_arch.upper() != "AARCH64":
            raise Exception("Invalid architecture specified; Barley supports AARCH64 only.")

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetPackagesPath (self):
        result = [shell_environment.GetBuildVars().GetValue("FEATURE_CONFIG_PATH", "")]

        for package_path in CommonPlatform.PackagesPath:
            result.append(package_path)

        return result

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def GetName (self):
        return "barleyPkg"

    def GetLoggingLevel (self, loggerType):
        return logging.INFO

    def SetPlatformEnv (self):
        logging.debug ("PlatformBuilder SetPlatformEnv")

        # EDK2's Windows wrappers require GnuWin32 Make and cmd.exe semantics.
        # Avoid an unrelated MSYS shell consuming native Windows paths when both
        # toolchains are installed on the host.
        if os.name == "nt":
            gnuwin_make = os.path.join(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"), "GnuWin32", "bin")
            if os.path.isfile(os.path.join(gnuwin_make, "make.exe")):
                path_entries = [
                    entry for entry in os.environ.get("PATH", "").split(os.pathsep)
                    if "devkitpro\\msys2" not in entry.lower().replace("/", "\\")
                ]
                os.environ["PATH"] = os.pathsep.join([gnuwin_make] + path_entries)

        self.env.SetValue ("PRODUCT_NAME", "barley", "Platform Hardcoded")
        self.env.SetValue ("ACTIVE_PLATFORM", "barleyPkg/barley.dsc", "Platform Hardcoded")
        self.env.SetValue ("TARGET_ARCH", "AARCH64", "Platform Hardcoded")
        self.env.SetValue ("TOOL_CHAIN_TAG", "CLANGPDB", "set default to clangpdb")
        self.env.SetValue ("EMPTY_DRIVE", "FALSE", "Default to false")
        self.env.SetValue ("RUN_TESTS", "FALSE", "Default to false")
        self.env.SetValue ("SHUTDOWN_AFTER_RUN", "FALSE", "Default to false")
        self.env.SetValue ("BLD_*_BUILDID_STRING", "Unknown", "Default")
        self.env.SetValue ("BUILDREPORTING", "TRUE", "Enabling build report")
        self.env.SetValue ("BUILDREPORT_TYPES", "PCD DEPEX FLASH BUILD_FLAGS LIBRARY FIXED_ADDRESS HASH", "Setting build report types")
        self.env.SetValue ("BLD_*_MEMORY_PROTECTION", "TRUE", "Default")
        self.env.SetValue ("BLD_*_SHIP_MODE", "FALSE", "Default")
        self.env.SetValue ("BLD_*_ENABLE_SECUREBOOT", self.env.GetValue("ENABLE_SECUREBOOT"), "Default")
        self.env.SetValue ("BLD_*_FD_BASE", self.env.GetValue("FD_BASE"), "Default")
        self.env.SetValue ("BLD_*_FD_SIZE", self.env.GetValue("FD_SIZE"), "Default")
        self.env.SetValue ("BLD_*_FD_BLOCKS", self.env.GetValue("FD_BLOCKS"), "Default")

        return 0

    def PlatformPreBuild (self):
        return 0

    def PlatformPostBuild (self):
        return 0

    def FlashRomImage (self):
        return 0


if __name__ == "__main__":
    import argparse
    import sys

    from edk2toolext.invocables.edk2_platform_build import Edk2PlatformBuild
    from edk2toolext.invocables.edk2_setup import Edk2PlatformSetup
    from edk2toolext.invocables.edk2_update import Edk2Update

    script_path = os.path.relpath (__file__)

    parser = argparse.ArgumentParser (add_help=False)
    parse_group = parser.add_mutually_exclusive_group()
    parse_group.add_argument ("--update", "--UPDATE", action='store_true', help="Invokes stuart_update")
    parse_group.add_argument ("--setup", "--SETUP", action='store_true', help="Invokes stuart_setup")
    args, remaining = parser.parse_known_args()

    new_args = ["stuart", "-c", script_path] + remaining
    sys.argv = new_args

    if args.setup:
        Edk2PlatformSetup().Invoke()
    elif args.update:
        Edk2Update().Invoke()
    else:
        Edk2PlatformBuild().Invoke()
