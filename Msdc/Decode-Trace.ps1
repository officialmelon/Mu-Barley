param([Parameter(Mandatory=$true)][string]$Path, [int]$HostIndex = 0)
$ErrorActionPreference = 'Stop'
$values = @{}
foreach ($line in Get-Content -LiteralPath $Path) {
    if ($line -match '^\s*(\S+)\s+REG_(DWORD|BINARY)\s+(\S+)') {
        $values[$Matches[1]] = $Matches[3]
    }
}
function Read-U32([string]$Hex, [int]$Offset) {
    if (!$Hex -or $Hex.Length -lt ($Offset + 4) * 2) { return $null }
    $word = $Hex.Substring(($Offset+3)*2,2) + $Hex.Substring(($Offset+2)*2,2) +
            $Hex.Substring(($Offset+1)*2,2) + $Hex.Substring($Offset*2,2)
    return [Convert]::ToUInt32($word,16)
}
$prefix = "DiagH$HostIndex"
if (!$values.ContainsKey("${prefix}TraceSequence")) { throw 'Trace sequence absent' }
$seq = [Convert]::ToInt32($values["${prefix}TraceSequence"].Substring(2),16)
$depth = @($values.Keys | Where-Object { $_ -match "^${prefix}Trace\d\dReq$" }).Count
if (!$depth) { throw 'Trace entries absent' }
"Host $HostIndex; issued sequence $seq; ring depth $depth. Live registry snapshots may overlap a request."
for ($i=[Math]::Max(0,$seq-$depth); $i -lt $seq; $i++) {
    $slot = $i % $depth
    $key = '{0}Trace{1:D2}' -f $prefix,$slot
    $raw = [Convert]::ToUInt32($values["${key}Req"].Substring(2),16)
    $status = Read-U32 $values["H${HostIndex}TraceStatus"] ($slot*4)
    [pscustomobject]@{
        Sequence=$i; Phase=(($raw -shr 28)-band 15); Command=(($raw -shr 22)-band 63)
        Argument=$values["${key}Arg"]; ResponseType=(($raw -shr 18)-band 15)
        TransferType=(($raw -shr 14)-band 15); BlockSize=($raw-band 16383)
        Blocks=(Read-U32 $values["H${HostIndex}TraceBlockCount"] ($slot*4))
        Length=(Read-U32 $values["H${HostIndex}TraceLength"] ($slot*4))
        Direction=(Read-U32 $values["H${HostIndex}TraceDirection"] ($slot*4))
        Status=$(if($null -eq $status){'not recorded'}else{'0x{0:X8}' -f $status})
        Response=(Read-U32 $values["H${HostIndex}TraceResponse"] ($slot*4))
        Interrupt=(Read-U32 $values["H${HostIndex}TraceInterrupt"] ($slot*4))
    }
}
$ext = $values["H${HostIndex}ExtCsd"]
if ($ext -and $ext.Length -eq 1024) {
    $sectors = Read-U32 $ext 212
    "Captured EXT_CSD: revision=$([Convert]::ToInt32($ext.Substring(384,2),16)); SEC_COUNT=$sectors; bytes=$([uint64]$sectors*512)"
    "Captured bus test sent=$($values["H${HostIndex}BusTestWrite"]); received=$($values["H${HostIndex}BusTestRead"])"
}
