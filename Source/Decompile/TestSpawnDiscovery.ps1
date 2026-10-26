[CmdletBinding()]
param(
    [string]$ReportPath = (Join-Path $PSScriptRoot 'spawn-discovery.txt')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedHash = '219525080e93e7a9d2b072b0dd75ed35bb7e41024e01e837c7889566723a9318'
$exe = Join-Path $PSScriptRoot '..\..\Game\Original\DESPERAB.EXE'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'The executable does not match the discovery baseline.'
}
$report = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $ReportPath).ProviderPath)
if (-not $report.Contains("SHA-256: $expectedHash") -or
    -not $report.Contains('Self-tests: PASS') -or $report.Contains('analysis budget reached')) {
    throw 'The report has the wrong identity, failed self-tests, or an incomplete budget-limited trace.'
}

# Independently enumerate relocation-backed far-call operands in the original on-disk image.
$bytes = [IO.File]::ReadAllBytes($exe)
$header = [BitConverter]::ToUInt16($bytes, 8) * 16
$table = [BitConverter]::ToUInt16($bytes, 24)
$count = [BitConverter]::ToUInt16($bytes, 6)
$expected = [Collections.Generic.HashSet[int]]::new()
for ($i = 0; $i -lt $count; ++$i) {
    $entry = $table + $i * 4
    $linear = [BitConverter]::ToUInt16($bytes, $entry + 2) * 16 +
        [BitConverter]::ToUInt16($bytes, $entry)
    $at = $header + $linear
    if ($linear -lt 3 -or $at + 1 -ge $bytes.Length -or $bytes[$at - 3] -ne 0x9a) { continue }
    $targetSegment = [BitConverter]::ToUInt16($bytes, $at)
    $targetOffset = [BitConverter]::ToUInt16($bytes, $at - 2)
    if ($targetSegment -eq 0x1e48 -and $targetOffset -in @(0x2049, 0x2085)) {
        [void]$expected.Add($linear - 3)
    }
}
$sites = [regex]::Matches($report, '(?ms)^SITE ([0-9A-F]{4}):([0-9A-F]{4}).*?(?=^SITE |^TOTAL:|\z)')
$actual = [Collections.Generic.HashSet[int]]::new()
foreach ($site in $sites) {
    $linear = [Convert]::ToInt32($site.Groups[1].Value, 16) * 16 +
        [Convert]::ToInt32($site.Groups[2].Value, 16)
    if (-not $actual.Add($linear) -or
        -not $site.Value.Contains('MZ relocation at far segment operand: true; near call: false')) {
        throw 'Unexpected duplicate or non-relocation-backed allocation match.'
    }
}
if ($expected.Count -ne 25 -or -not $actual.SetEquals($expected) -or
    -not $report.Contains('TOTAL: 25 direct-call encoding matches; 24 local-CFG sites; 1 unresolved candidates.')) {
    throw 'The allocator index differs from independent enumeration or its known baseline.'
}
$unvalidated = @($sites | Where-Object { $_.Value.Contains('CANDIDATE ONLY') })
if ($unvalidated.Count -ne 1 -or -not $unvalidated[0].Value.StartsWith('SITE 0000:B070')) {
    throw 'Unexpected local-CFG validation coverage.'
}
if ($unvalidated[0].Value.Contains('field +08')) {
    throw 'Unvalidated call boundaries must not produce promoted object assignments.'
}

$ids = @([regex]::Matches($report, 'field \+08 BEHAVIOR WORD = (\d+)') |
    ForEach-Object { [int]$_.Groups[1].Value } | Sort-Object -Unique)
if (($ids -join ',') -ne '1,2,3,4,18,21,24,33,41,42,103,106,114,143') {
    throw 'The discovered selector set differs from the instruction-checked baseline.'
}
$stores = @([regex]::Matches($report,
    '([0-9A-F]{4}:[0-9A-F]{4}) field \+08 BEHAVIOR WORD = \d+') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
if ($stores.Count -ne 19 -or -not $report.Contains(
    '10AC:09AC field +08 BEHAVIOR WORD = 21 (0x0015)')) {
    throw 'The unique store count or known fireplace positive control differs.'
}
foreach ($edge in @(
    'SLOT ARGUMENT 0000:2477 -> 10AC:044E',
    'SLOT ARGUMENT 0000:358F -> 10AC:D682',
    'SLOT ARGUMENT 0000:A99F -> 10AC:0384'
)) {
    if (-not $report.Contains($edge)) { throw "Missing initializer edge: $edge" }
}
if ($report -match 'SLOT ARGUMENT [^\r\n]+ -> (2CE4:170A|1E48:0AA0)') {
    throw 'A slot pending below random-helper arguments was incorrectly treated as consumed.'
}

$assets = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot '..\..\Data\SourceAssets\Original\LEVEL') `
    -Filter 'OBJPOS*.DAT'
$counts = @{}
$records = 0
foreach ($asset in $assets) {
    $data = [IO.File]::ReadAllBytes($asset.FullName)
    if ($data.Length % 54 -ne 0) { throw "Invalid 54-byte object table: $($asset.Name)" }
    $records += $data.Length / 54
    for ($at = 8; $at -lt $data.Length; $at += 54) {
        $id = [int][BitConverter]::ToUInt16($data, $at)
        if (-not $counts.ContainsKey($id)) { $counts[$id] = 0 }
        ++$counts[$id]
    }
}
if ($assets.Count -ne 38 -or $records -ne 2920) { throw 'The initial-object asset baseline changed.' }
foreach ($id in $ids) {
    $actualCount = if ($counts.ContainsKey($id)) { $counts[$id] } else { 0 }
    $expectedCount = if ($id -eq 41) { 21 } elseif ($id -eq 42) { 1 } else { 0 }
    if ($actualCount -ne $expectedCount) { throw "Unexpected initial asset count for behavior $id." }
}
Write-Output 'PASS: 25 allocator sites independently matched; 24 local CFG paths; one explicit candidate.'
Write-Output 'PASS: 19 unique behavior stores / 14 selectors; fireplace and forwarding controls.'
Write-Output 'PASS: 38 original object tables / 2920 records; 12 discovered selectors absent initially.'
