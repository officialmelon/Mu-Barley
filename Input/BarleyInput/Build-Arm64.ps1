[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidatePattern('^[0-9A-Fa-f]{40}$')]
    [string]$SigningThumbprint = ''
)

$ErrorActionPreference = 'Stop'

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Workspace = Resolve-Path (Join-Path $ProjectDir '..\..\..')
$Wdk = Join-Path $Workspace 'Tools\WDK-NuGet\10.0.26100.6584-arm64'
$KitVersion = '10.0.26100.0'
$KmdfVersion = '1.33'
$MsvcToolsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC'
$MsvcTools = Get-ChildItem -LiteralPath $MsvcToolsRoot -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
if ($null -eq $MsvcTools) { throw 'Microsoft Visual C++ tools were not found' }
$Compiler = Join-Path $MsvcTools.FullName 'bin\Hostx64\arm64\cl.exe'
$Linker = Join-Path $MsvcTools.FullName 'bin\Hostx64\arm64\link.exe'
if (-not (Test-Path -LiteralPath $Compiler)) {
    throw "Microsoft ARM64 compiler was not found at $Compiler"
}
if (-not (Test-Path -LiteralPath $Linker)) {
    throw "Microsoft ARM64 linker was not found at $Linker"
}

$OutDir = Join-Path $ProjectDir "out\ARM64\$Configuration"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$KmInclude = Join-Path $Wdk "c\Include\$KitVersion\km"
$InstalledSdkInclude = "C:\Program Files (x86)\Windows Kits\10\Include\$KitVersion"
$SharedInclude = Join-Path $InstalledSdkInclude 'shared'
$UcrtInclude = Join-Path $InstalledSdkInclude 'ucrt'
$WdfInclude = Join-Path $Wdk "c\Include\wdf\kmdf\$KmdfVersion"
$MsvcInclude = Join-Path $MsvcTools.FullName 'include'
$KmLib = Join-Path $Wdk "c\Lib\$KitVersion\km\ARM64"
$WdfLib = Join-Path $Wdk "c\Lib\wdf\kmdf\ARM64\$KmdfVersion"
$Object = Join-Path $OutDir 'barley_input.obj'
$Driver = Join-Path $OutDir 'BarleyInput.sys'

$Compile = @(
    '/nologo', '/c', '/kernel', '/Zl', '/GS', '/Gy', '/Oi', '/W4',
    '/guard:cf', '/volatile:iso', '/Zp8',
    '/D_ARM64_', '/DARM64', '/D_WIN64',
    '/DUNICODE', '/D_UNICODE', '/DPOOL_NX_OPTIN=1',
    '/DNTDDI_VERSION=0x0A00000C', '/D_WIN32_WINNT=0x0A00',
    "/I$KmInclude", "/I$SharedInclude", "/I$UcrtInclude", "/I$WdfInclude", "/I$MsvcInclude",
    (Join-Path $ProjectDir 'barley_input.c'),
    "/Fo$Object"
)
if ($Configuration -eq 'Debug') {
    $Compile += @('/Od', '/Zi')
} else {
    $Compile += @('/O2')
}

& $Compiler @Compile
if ($LASTEXITCODE -ne 0) { throw "cl.exe failed with exit code $LASTEXITCODE" }

$Link = @(
    '/nologo', '/driver', '/release', '/Brepro', '/subsystem:native,6.2', '/osversion:10.0', '/stack:0x40000,0x1000', '/machine:arm64',
    '/entry:FxDriverEntry', '/nodefaultlib',
    "/out:$Driver"
)
$Link += @('/base:0x1C0000000', '/guard:cf', '/incremental:no',
    '/opt:ref', '/opt:icf', '/dynamicbase', '/nxcompat')
$Link = $Link + @($Object,
    "/libpath:$KmLib", "/libpath:$WdfLib",
    'wdfdriverentry.lib', 'wdfldr.lib', 'ntoskrnl.lib', 'hal.lib',
    'BufferOverflowFastFailK.lib'
)
if ($Configuration -eq 'Debug') {
    $Link += @('/debug', "/pdb:$(Join-Path $OutDir 'BarleyInput.pdb')")
}

& $Linker @Link
if ($LASTEXITCODE -ne 0) { throw "link failed with exit code $LASTEXITCODE" }

$SignTool = "C:\Program Files (x86)\Windows Kits\10\bin\$KitVersion\x64\signtool.exe"
if (-not [string]::IsNullOrWhiteSpace($SigningThumbprint)) {
    if (-not (Test-Path -LiteralPath $SignTool)) {
        throw "signtool was not found at $SignTool"
    }
    & $SignTool sign /v /s My /sha1 $SigningThumbprint /fd SHA256 $Driver
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed to sign the driver: $LASTEXITCODE"
    }
}

Copy-Item -LiteralPath (Join-Path $ProjectDir 'BarleyInput.inf') -Destination $OutDir -Force

$PackageDir = Join-Path $OutDir 'package'
if (Test-Path -LiteralPath $PackageDir) {
    Remove-Item -LiteralPath $PackageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Copy-Item -LiteralPath $Driver -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectDir 'BarleyInput.inf') -Destination $PackageDir -Force

$Inf2Cat = Join-Path $Wdk "c\bin\$KitVersion\x86\Inf2Cat.exe"
& $Inf2Cat "/driver:$PackageDir" '/os:10_GE_ARM64'
if ($LASTEXITCODE -ne 0) { throw "Inf2Cat failed with exit code $LASTEXITCODE" }

$Catalog = Join-Path $PackageDir 'BarleyInput.cat'
if (-not [string]::IsNullOrWhiteSpace($SigningThumbprint)) {
    & $SignTool sign /v /s My /sha1 $SigningThumbprint /fd SHA256 $Catalog
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed to sign the catalog: $LASTEXITCODE"
    }

    foreach ($SignedFile in @($Driver, (Join-Path $PackageDir 'BarleyInput.sys'), $Catalog)) {
        $Signature = Get-AuthenticodeSignature -LiteralPath $SignedFile
        if ($Signature.SignerCertificate.Thumbprint -ne $SigningThumbprint) {
            throw "Unexpected or missing test signature on $SignedFile"
        }
    }
}

$ReadObj = (Get-Command llvm-readobj -ErrorAction Stop).Source
& $ReadObj --file-headers $Driver
Get-FileHash -LiteralPath $Driver -Algorithm SHA256
Get-FileHash -LiteralPath $Catalog -Algorithm SHA256
