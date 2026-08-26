[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$FirmwarePath = '',

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

if ([string]::IsNullOrWhiteSpace($FirmwarePath)) {
    $FirmwarePath = Join-Path $Workspace `
        'CODEX-OUTBOX\barley-touch-stock\Himax_firmware_boe.bin'
}
$FirmwarePath = (Resolve-Path -LiteralPath $FirmwarePath).Path
$Firmware = Get-Item -LiteralPath $FirmwarePath
if ($Firmware.Length -ne 261120) {
    throw "Unexpected HX83102J firmware size: $($Firmware.Length)"
}
$FirmwareHash = (Get-FileHash -LiteralPath $FirmwarePath -Algorithm SHA256).Hash
if ($FirmwareHash -ne `
    '58AE7D487EF16A43DE068A85DED0913E471F4AB806C053EA76611E3B1B21353D') {
    throw "Unexpected HX83102J firmware SHA256: $FirmwareHash"
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

$CommonCompile = @(
    '/nologo', '/c', '/kernel', '/Zl', '/GS', '/Gy', '/Oi', '/W4',
    '/guard:cf', '/volatile:iso', '/Zp8',
    '/D_ARM64_', '/DARM64', '/D_WIN64',
    '/DUNICODE', '/D_UNICODE', '/DPOOL_NX_OPTIN=1',
    "/DKMDF_VERSION_MAJOR=1", "/DKMDF_VERSION_MINOR=33",
    '/DNTDDI_VERSION=0x0A00000C', '/D_WIN32_WINNT=0x0A00',
    "/I$KmInclude", "/I$SharedInclude",
    "/I$UcrtInclude", "/I$WdfInclude", "/I$MsvcInclude"
)
if ($Configuration -eq 'Debug') {
    $CommonCompile += @('/Od', '/Zi')
} else {
    $CommonCompile += @('/O2')
}

$Objects = @()
foreach ($SourceName in @('barley_touch.c', 'mtk_spi.c', 'himax_hx83102j.c')) {
    $Object = Join-Path $OutDir `
        (([IO.Path]::GetFileNameWithoutExtension($SourceName)) + '.obj')
    $Compile = $CommonCompile + @(
        (Join-Path $ProjectDir $SourceName),
        "/Fo$Object"
    )
    & $Compiler @Compile
    if ($LASTEXITCODE -ne 0) {
        throw "cl.exe failed for $SourceName with exit code $LASTEXITCODE"
    }
    $Objects += $Object
}

$Driver = Join-Path $OutDir 'BarleyHimaxTouch.sys'
$Link = @(
    '/nologo', '/driver', '/release', '/Brepro', '/subsystem:native,6.2',
    '/osversion:10.0', '/stack:0x40000,0x1000',
    '/machine:arm64', '/entry:FxDriverEntry', '/nodefaultlib',
    "/out:$Driver"
)
$Link += @('/base:0x1C0000000', '/guard:cf', '/incremental:no',
    '/opt:ref', '/opt:icf', '/dynamicbase', '/nxcompat')
$Link = $Link + $Objects + @(
    "/libpath:$KmLib", "/libpath:$WdfLib",
    'wdfdriverentry.lib', 'wdfldr.lib', 'vhfkm.lib',
    'ntoskrnl.lib', 'hal.lib', 'libcntpr.lib',
    'BufferOverflowFastFailK.lib'
)
if ($Configuration -eq 'Debug') {
    $Link += @('/debug', "/pdb:$(Join-Path $OutDir 'BarleyHimaxTouch.pdb')")
}

& $Linker @Link
if ($LASTEXITCODE -ne 0) {
    throw "link failed with exit code $LASTEXITCODE"
}

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

$PackageDir = Join-Path $OutDir 'package'
if (Test-Path -LiteralPath $PackageDir) {
    Remove-Item -LiteralPath $PackageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Copy-Item -LiteralPath $Driver -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectDir 'BarleyHimaxTouch.inf') `
    -Destination $PackageDir -Force
Copy-Item -LiteralPath $FirmwarePath `
    -Destination (Join-Path $PackageDir 'Himax_firmware_boe.bin') -Force

$Inf2Cat = Join-Path $Wdk "c\bin\$KitVersion\x86\Inf2Cat.exe"
& $Inf2Cat "/driver:$PackageDir" '/os:10_GE_ARM64'
if ($LASTEXITCODE -ne 0) {
    throw "Inf2Cat failed with exit code $LASTEXITCODE"
}

$Catalog = Join-Path $PackageDir 'BarleyHimaxTouch.cat'
if (-not [string]::IsNullOrWhiteSpace($SigningThumbprint)) {
    & $SignTool sign /v /s My /sha1 $SigningThumbprint /fd SHA256 $Catalog
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed to sign the catalog: $LASTEXITCODE"
    }

    foreach ($SignedFile in @($Driver, (Join-Path $PackageDir 'BarleyHimaxTouch.sys'), $Catalog)) {
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
Get-FileHash -LiteralPath `
    (Join-Path $PackageDir 'Himax_firmware_boe.bin') -Algorithm SHA256
