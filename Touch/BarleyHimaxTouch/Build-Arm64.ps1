[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$FirmwarePath = ''
)

$ErrorActionPreference = 'Stop'

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Workspace = Resolve-Path (Join-Path $ProjectDir '..\..\..')
$Wdk = Join-Path $Workspace 'Tools\WDK-NuGet\10.0.26100.6584-arm64'
$KitVersion = '10.0.26100.0'
$KmdfVersion = '1.33'
$ClangCl = (Get-Command clang -ErrorAction Stop).Source
$LldLink = (Get-Command lld-link -ErrorAction Stop).Source

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
$MsvcToolsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC'
$MsvcInclude = Join-Path (
    Get-ChildItem -LiteralPath $MsvcToolsRoot -Directory |
        Sort-Object Name -Descending |
        Select-Object -First 1 -ExpandProperty FullName
) 'include'
$KmLib = Join-Path $Wdk "c\Lib\$KitVersion\km\ARM64"
$WdfLib = Join-Path $Wdk "c\Lib\wdf\kmdf\ARM64\$KmdfVersion"

# WDK 26100's ARM64 wdm.h relies on MSVC's legacy empty-token-paste
# extension in one macro.  Create a derived compatibility header in the
# output directory and leave the installed/NuGet WDK immutable.
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

$CommonCompile = @(
    '--driver-mode=cl',
    '--target=arm64-pc-windows-msvc',
    '/nologo', '/c', '/kernel', '/Zl', '/GS', '/Gy', '/Oi', '/W4',
    '/D_ARM64_', '/DARM64', '/D_WIN64', '/D_KERNEL_MODE',
    '/DUNICODE', '/D_UNICODE', '/DPOOL_NX_OPTIN=1',
    '/DNTDDI_VERSION=0x0A00000C', '/D_WIN32_WINNT=0x0A00',
    "/I$CompatInclude", "/I$KmInclude", "/I$SharedInclude",
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
    & $ClangCl @Compile
    if ($LASTEXITCODE -ne 0) {
        throw "clang-cl failed for $SourceName with exit code $LASTEXITCODE"
    }
    $Objects += $Object
}

$Driver = Join-Path $OutDir 'BarleyHimaxTouch.sys'
$Link = @(
    '/nologo', '/driver', '/release', '/Brepro', '/subsystem:native',
    '/machine:arm64', '/entry:FxDriverEntry', '/nodefaultlib', '/guard:cf',
    "/out:$Driver"
) + $Objects + @(
    "/libpath:$KmLib", "/libpath:$WdfLib",
    'wdfdriverentry.lib', 'wdfldr.lib', 'vhfkm.lib',
    'ntoskrnl.lib', 'hal.lib', 'libcntpr.lib',
    'BufferOverflowFastFailK.lib'
)
if ($Configuration -eq 'Debug') {
    $Link += @('/debug', "/pdb:$(Join-Path $OutDir 'BarleyHimaxTouch.pdb')")
}

& $LldLink @Link
if ($LASTEXITCODE -ne 0) {
    throw "lld-link failed with exit code $LASTEXITCODE"
}

$PackageDir = Join-Path $OutDir 'package'
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

$ReadObj = (Get-Command llvm-readobj -ErrorAction Stop).Source
& $ReadObj --file-headers $Driver
Get-FileHash -LiteralPath $Driver -Algorithm SHA256
Get-FileHash -LiteralPath `
    (Join-Path $PackageDir 'BarleyHimaxTouch.cat') -Algorithm SHA256
Get-FileHash -LiteralPath `
    (Join-Path $PackageDir 'Himax_firmware_boe.bin') -Algorithm SHA256
