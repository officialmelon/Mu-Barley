$ErrorActionPreference = 'Stop'
$driverRoot = $PSScriptRoot
$workspace = Split-Path (Split-Path $driverRoot -Parent) -Parent
$reference = Get-Content (Join-Path $workspace 'Tools/windows-driver-samples/sd/miniport/sdhc/sdhc.inx') -Raw
$candidate = Get-Content (Join-Path $driverRoot 'mtkmsdc.inx') -Raw
function Get-Policy([string]$Inf, [string]$Name) {
    $logical = $Inf -replace '\\\r?\n\s*', ''
    $match = [regex]::Match($logical, "(?im)^HKR,Parameters,$Name,1,([^\r\n]+)")
    if (!$match.Success) { throw "Missing $Name" }
    return (($match.Groups[1].Value -replace '\s','').ToUpperInvariant())
}
if ($candidate -notmatch '(?s)\[Msdc_Service\][^\[]*AddReg=Msdc_ServiceParameters') {
    throw 'Host service does not install its command policies'
}
if ($candidate -notmatch '(?im)^LoadOrderGroup\s*=\s*System Bus Extender\s*$') {
    throw 'Storage miniport load-order group is missing'
}
if ($candidate -notmatch '(?im)^BootFlags\s*=\s*0x00000008\s*$') {
    throw 'Boot-volume promotion flag is missing'
}
Write-Output 'PASS: boot-storage load order matches the Microsoft SDHC sample'
foreach ($name in 'SdCmdFlags','SdAppCmdFlags') {
    if ((Get-Policy $candidate $name) -ne (Get-Policy $reference $name)) {
        throw "$name differs from the Microsoft reference policy"
    }
    Write-Output "PASS: $name matches Microsoft reference byte-for-byte"
}
$source = Get-Content (Join-Path $driverRoot 'mtkmsdc.c') -Raw
$cmd3 = [regex]::Match($source, '(?s)if \(Extension->IsEmmc != FALSE &&\s*Command->Index == 3.*?MtkMsdcSetBits\(Extension, MSDC_INTEN').Value
if ($cmd3 -notmatch 'SdPortCompleteRequest\(Request, Status\);\s*/\*[^*]*\*/\s*return STATUS_PENDING;') {
    throw 'CMD3 accepted-request completion contract missing'
}
Write-Output 'PASS: CMD3 reports actual completion status and returns PENDING'
$response = [regex]::Match($source, '(?s)VOID\s+MtkMsdcGetResponse\(.*?(?=_Use_decl_annotations_)').Value
if ($response -match 'Extension->Response\[\d\]\s*=') { throw 'GetResponse mutates cached hardware data' }
foreach ($forbidden in 'SC128','A3A562','25015ba5','8ef08bff','400e0032','SdPortPropertyMask') {
    if ($source -match [regex]::Escape($forbidden)) { throw "Card-specific/patch marker: $forbidden" }
}
Write-Output 'PASS: response cache immutable; no known card-specific replacement markers'
$capture = [regex]::Match($source, '(?s)MtkMsdcCaptureResponse\(.*?(?=static NTSTATUS)').Value
if ($capture -notmatch 'if \(LongResponse != FALSE\)\s*\{\s*MtkMsdcWaitResponseLatched\(Extension\);') {
    throw 'Response settling must be restricted to R2; data commands cannot wait for FIFO idle'
}
if ($source -notmatch 'MtkMsdcCaptureResponse\(\s*Extension,\s*Request->Command.ResponseType == SdResponseTypeR2\)') {
    throw 'DPC response capture must select the R2 settling path from the request type'
}
Write-Output 'PASS: only long R2 responses wait for datapath settling'
Write-Output 'Static package/source checks only; hardware enumeration is NOT established by this test.'
