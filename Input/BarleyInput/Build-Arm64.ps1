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
$ClangCl = (Get-Command clang -ErrorAction Stop).Source
$LldLink = (Get-Command lld-link -ErrorAction Stop).Source

if (-not (Test-Path -LiteralPath $ClangCl)) {
    throw "clang-cl was not found at $ClangCl"
}
if (-not (Test-Path -LiteralPath $LldLink)) {
    throw "lld-link was not found at $LldLink"
}

$OutDir = Join-Path $ProjectDir "out\ARM64\$Configuration"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$KmInclude = Join-Path $Wdk "c\Include\$KitVersion\km"
$InstalledSdkInclude = "C:\Program Files (x86)\Windows Kits\10\Include\$KitVersion"
$SharedInclude = Join-Path $InstalledSdkInclude 'shared'
$UcrtInclude = Join-Path $InstalledSdkInclude 'ucrt'
$WdfInclude = Join-Path $Wdk "c\Include\wdf\kmdf\$KmdfVersion"
$MsvcToolsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC'
$MsvcInclude = Join-Path (
    Get-ChildItem -LiteralPath $MsvcToolsRoot -Directory |
        Sort-Object Name -Descending |
        Select-Object -First 1 -ExpandProperty FullName
) 'include'
$KmLib = Join-Path $Wdk "c\Lib\$KitVersion\km\ARM64"
$WdfLib = Join-Path $Wdk "c\Lib\wdf\kmdf\ARM64\$KmdfVersion"
$Object = Join-Path $OutDir 'barley_input.obj'
$Driver = Join-Path $OutDir 'BarleyInput.sys'

# WDK 26100's ARM64 wdm.h uses MSVC's legacy empty-token-paste extension in
# one macro (`&##_variable`). Clang correctly rejects that token paste. Create
# a derived compatibility header in the output tree; never alter the WDK.
$CompatInclude = Join-Path $OutDir 'generated-include'
New-Item -ItemType Directory -Force -Path $CompatInclude | Out-Null
$WdmSource = Join-Path $KmInclude 'wdm.h'
$WdmText = [IO.File]::ReadAllText($WdmSource)
$WdmCompatText = $WdmText.Replace('&##_variable', '&_variable')
if ($WdmCompatText -eq $WdmText) {
    throw 'Expected WDK ARM64 token-paste construct was not found in wdm.h'
}
[IO.File]::WriteAllText(
    (Join-Path $CompatInclude 'wdm.h'),
    $WdmCompatText,
    [Text.UTF8Encoding]::new($false))

$Compile = @(
    '--driver-mode=cl',
    '--target=arm64-pc-windows-msvc',
    '/nologo', '/c', '/kernel', '/Zl', '/GS', '/Gy', '/Oi', '/W4',
    '/D_ARM64_', '/DARM64', '/D_WIN64', '/D_KERNEL_MODE',
    '/DUNICODE', '/D_UNICODE', '/DPOOL_NX_OPTIN=1',
    '/DNTDDI_VERSION=0x0A00000C', '/D_WIN32_WINNT=0x0A00',
    "/I$CompatInclude", "/I$KmInclude", "/I$SharedInclude", "/I$UcrtInclude", "/I$WdfInclude", "/I$MsvcInclude",
    (Join-Path $ProjectDir 'barley_input.c'),
    "/Fo$Object"
)
if ($Configuration -eq 'Debug') {
    $Compile += @('/Od', '/Zi')
} else {
    $Compile += @('/O2')
}

& $ClangCl @Compile
if ($LASTEXITCODE -ne 0) { throw "clang-cl failed with exit code $LASTEXITCODE" }

$Link = @(
    '/nologo', '/driver', '/release', '/Brepro', '/subsystem:native', '/machine:arm64',
    '/entry:FxDriverEntry', '/nodefaultlib', '/guard:cf',
    "/out:$Driver", $Object,
    "/libpath:$KmLib", "/libpath:$WdfLib",
    'wdfdriverentry.lib', 'wdfldr.lib', 'ntoskrnl.lib', 'hal.lib',
    'BufferOverflowFastFailK.lib'
)
if ($Configuration -eq 'Debug') {
    $Link += @('/debug', "/pdb:$(Join-Path $OutDir 'BarleyInput.pdb')")
}

& $LldLink @Link
if ($LASTEXITCODE -ne 0) { throw "lld-link failed with exit code $LASTEXITCODE" }

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
