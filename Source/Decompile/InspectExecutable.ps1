[CmdletBinding()]
param(
    [string]$ExecutablePath = (Join-Path $PSScriptRoot '..\..\Game\Original\DESPERAB.EXE'),
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedPath = (Resolve-Path -LiteralPath $ExecutablePath).ProviderPath
$bytes = [System.IO.File]::ReadAllBytes($resolvedPath)
if ($bytes.Length -lt 28) {
    throw 'The executable is too short to contain a DOS MZ header.'
}
if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
    throw 'The executable does not have an MZ signature.'
}

$lastPageBytes = [BitConverter]::ToUInt16($bytes, 2)
$pages = [BitConverter]::ToUInt16($bytes, 4)
$relocationCount = [BitConverter]::ToUInt16($bytes, 6)
$headerBytes = [BitConverter]::ToUInt16($bytes, 8) * 16
$initialSS = [BitConverter]::ToUInt16($bytes, 14)
$initialSP = [BitConverter]::ToUInt16($bytes, 16)
$initialIP = [BitConverter]::ToUInt16($bytes, 20)
$initialCS = [BitConverter]::ToUInt16($bytes, 22)
$relocationTableOffset = [BitConverter]::ToUInt16($bytes, 24)
$overlayNumber = [BitConverter]::ToUInt16($bytes, 26)

if ($pages -eq 0 -or $lastPageBytes -gt 511) {
    throw 'The MZ image size fields are invalid.'
}
$declaredImageBytes = $pages * 512
if ($lastPageBytes -ne 0) {
    $declaredImageBytes -= 512 - $lastPageBytes
}
if ($headerBytes -lt 28 -or $headerBytes -gt $declaredImageBytes -or $declaredImageBytes -gt $bytes.Length) {
    throw 'The MZ header or declared image extends outside the executable.'
}
if ($relocationCount -gt 0 -and
    ($relocationTableOffset -lt 28 -or $relocationTableOffset + $relocationCount * 4 -gt $headerBytes)) {
    throw 'The relocation table is outside the MZ header.'
}

$loadModuleBytes = $declaredImageBytes - $headerBytes
for ($i = 0; $i -lt $relocationCount; $i++) {
    $entry = $relocationTableOffset + $i * 4
    $offset = [BitConverter]::ToUInt16($bytes, $entry)
    $segment = [BitConverter]::ToUInt16($bytes, $entry + 2)
    if ($segment * 16 + $offset + 2 -gt $loadModuleBytes) {
        throw "Relocation $i targets a word outside the on-disk load module."
    }
}

$entryModuleOffset = $initialCS * 16 + $initialIP
if ($entryModuleOffset -ge $loadModuleBytes) {
    throw 'The entry point is outside the on-disk load module.'
}
$entryFileOffset = $headerBytes + $entryModuleOffset
$entryByteCount = [Math]::Min(32, $declaredImageBytes - $entryFileOffset)

$report = [ordered]@{
    schemaVersion = 1
    fileName = [System.IO.Path]::GetFileName($resolvedPath)
    sha256 = (Get-FileHash -LiteralPath $resolvedPath -Algorithm SHA256).Hash.ToLowerInvariant()
    fileBytes = $bytes.Length
    format = 'DOS MZ'
    headerBytes = $headerBytes
    declaredImageBytes = $declaredImageBytes
    loadModuleBytes = $loadModuleBytes
    trailingBytes = $bytes.Length - $declaredImageBytes
    relocationTableFileOffset = $relocationTableOffset
    relocationCount = $relocationCount
    relocationTargetsValidated = $true
    overlayNumber = $overlayNumber
    minimumExtraParagraphs = [BitConverter]::ToUInt16($bytes, 10)
    maximumExtraParagraphs = [BitConverter]::ToUInt16($bytes, 12)
    entryPoint = [ordered]@{
        moduleRelativeSegmentOffset = '{0:X4}:{1:X4}' -f $initialCS, $initialIP
        moduleLinearOffset = $entryModuleOffset
        fileOffset = $entryFileOffset
        bytes = [BitConverter]::ToString($bytes, $entryFileOffset, $entryByteCount).Replace('-', ' ')
    }
    initialStack = [ordered]@{
        moduleRelativeSegment = '{0:X4}' -f $initialSS
        offset = '{0:X4}' -f $initialSP
    }
}

$json = $report | ConvertTo-Json -Depth 4
if ($OutputPath) {
    $fullOutputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
    if ([string]::Equals($resolvedPath, $fullOutputPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The report output path must not overwrite the executable.'
    }
    [System.IO.File]::WriteAllText($fullOutputPath, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
}
$json
