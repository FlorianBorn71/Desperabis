[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DisassemblyPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedHash = '219525080e93e7a9d2b072b0dd75ed35bb7e41024e01e837c7889566723a9318'
$exe = Join-Path $PSScriptRoot '..\..\Game\Original\DESPERAB.EXE'
$header = Join-Path $PSScriptRoot '..\NewRenderer\SharedSource\base\GamePlay\GamePlay.h'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'The executable does not match the analyzed original.'
}
$text = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $DisassemblyPath).ProviderPath)
if (-not $text.StartsWith("SHA-256: $expectedHash") -or $text -match 'DECODE STOP|STOP:') {
    throw 'Expected a complete InspectCode export for the original executable.'
}

$bytes = [System.IO.File]::ReadAllBytes($exe)
$headerBytes = [BitConverter]::ToUInt16($bytes, 8) * 16
$relocationCount = [BitConverter]::ToUInt16($bytes, 6)
$relocationTable = [BitConverter]::ToUInt16($bytes, 24)
$relocated = [byte[]]$bytes.Clone()
for ($i = 0; $i -lt $relocationCount; $i++) {
    $record = $relocationTable + $i * 4
    $at = $headerBytes + [BitConverter]::ToUInt16($bytes, $record + 2) * 16 +
        [BitConverter]::ToUInt16($bytes, $record)
    $value = ([BitConverter]::ToUInt16($relocated, $at) + 0x1000) -band 0xffff
    $relocated[$at] = $value -band 0xff
    $relocated[$at + 1] = $value -shr 8
}
$instructions = @{}
foreach ($line in ($text -split '\r?\n')) {
    if ($line -match '^0000:([0-9A-F]{4})\s+((?:[0-9A-F]{2} )+)\s+(.+)$') {
        $offset = [Convert]::ToInt32($Matches[1], 16)
        $raw = [byte[]]@($Matches[2].Trim().Split(' ') | ForEach-Object { [Convert]::ToByte($_, 16) })
        $display = $Matches[3]
        for ($j = 0; $j -lt $raw.Length; $j++) {
            if ($relocated[$headerBytes + $offset + $j] -ne $raw[$j]) {
                throw ('Export bytes differ from the relocated executable at {0:X4}.' -f ($offset + $j))
            }
        }
        if ($instructions.ContainsKey($offset)) {
            throw ('Duplicate instruction at {0:X4}.' -f $offset)
        }
        $instructions[$offset] = @{ Offset = $offset; Bytes = $raw; Text = $display }
    }
}
for ($at = 0x1094; $at -lt 0x551f;) {
    if (-not $instructions.ContainsKey($at)) {
        throw ('Missing instruction at {0:X4}.' -f $at)
    }
    $at += $instructions[$at].Bytes.Length
}
if ($at -ne 0x551f) { throw 'The object routine export has an invalid end boundary.' }

function Get-RelativeTarget($instruction) {
    $raw = $instruction.Bytes
    $op = [int]$raw[0]
    if (($op -eq 0xe8 -or $op -eq 0xe9) -and $raw.Length -eq 3) {
        $delta = [BitConverter]::ToInt16($raw, 1)
    } elseif (($op -eq 0xeb -or ($op -ge 0x70 -and $op -le 0x7f) -or
                ($op -ge 0xe0 -and $op -le 0xe3)) -and $raw.Length -eq 2) {
        $delta = [int]$raw[1]
        if ($delta -gt 127) { $delta -= 256 }
    } elseif ($op -eq 0x0f -and $raw.Length -eq 4 -and $raw[1] -ge 0x80 -and $raw[1] -le 0x8f) {
        $delta = [BitConverter]::ToInt16($raw, 2)
    } else {
        throw ('Unsupported relative branch at {0:X4}.' -f $instruction.Offset)
    }
    return ($instruction.Offset + $raw.Length + $delta) -band 0xffff
}

function Trace-Selector([int]$behavior) {
    $ip = 0x1151
    $compared = $null
    $seen = @{}
    $comparisons = [System.Collections.Generic.List[int]]::new()
    while ($ip -ne 0x5110) {
        if ($seen.ContainsKey($ip) -or -not $instructions.ContainsKey($ip)) {
            throw ('Invalid comparison-cascade path at {0:X4}.' -f $ip)
        }
        $seen[$ip] = $true
        $instruction = $instructions[$ip]
        $raw = $instruction.Bytes
        $op = [int]$raw[0]
        if ($op -eq 0x3d) {
            $compared = [int][BitConverter]::ToUInt16($raw, 1)
            $comparisons.Add($compared)
            $ip += 3
        } elseif ($op -eq 0x83 -and $raw[1] -eq 0xf8) {
            $compared = [int]$raw[2]
            if ($compared -gt 127) { $compared -= 256 }
            $compared = $compared -band 0xffff
            $comparisons.Add($compared)
            $ip += 3
        } elseif ($op -eq 0xe9 -or $op -eq 0xeb) {
            $ip = Get-RelativeTarget $instruction
        } elseif ($op -eq 0x74 -or $op -eq 0x75) {
            if ($null -eq $compared) { throw 'Branch has no selector comparison.' }
            $take = $behavior -eq $compared
            if ($op -eq 0x75) { $take = -not $take }
            if ($take) { $ip = Get-RelativeTarget $instruction } else { $ip += $raw.Length }
        } else {
            break
        }
    }
    return @{ Entry = $ip; Comparisons = $comparisons.ToArray() }
}

function Get-CaseReferences([int]$entry) {
    $pending = [System.Collections.Generic.Stack[int]]::new()
    $pending.Push($entry)
    $seen = @{}
    $globals = @{}
    $calls = @{}
    while ($pending.Count -gt 0) {
        $ip = $pending.Pop()
        if ($ip -eq 0x5110 -or $seen.ContainsKey($ip)) { continue }
        if ($ip -lt 0x1156 -or $ip -ge 0x5110 -or -not $instructions.ContainsKey($ip)) {
            throw ('Case reaches an unexpected boundary at {0:X4}.' -f $ip)
        }
        $seen[$ip] = $true
        $instruction = $instructions[$ip]
        $raw = $instruction.Bytes
        $op = [int]$raw[0]
        foreach ($match in [regex]::Matches($instruction.Text, '(?<!:)\[0x([0-9a-f]+)\]')) {
            $address = [Convert]::ToInt32($match.Groups[1].Value, 16)
            $globals[$address] = $true
        }
        if ($op -eq 0x9a -and $raw.Length -eq 5) {
            $segment = [int][BitConverter]::ToUInt16($raw, 3) - 0x1000
            $target = [BitConverter]::ToUInt16($raw, 1)
            $calls[('{0:X4}:{1:X4}' -f $segment, $target)] = $true
        } elseif ($op -eq 0xe8) {
            $calls[('0000:{0:X4}' -f (Get-RelativeTarget $instruction))] = $true
        }
        if ($op -eq 0xe9 -or $op -eq 0xeb) {
            $pending.Push((Get-RelativeTarget $instruction))
            continue
        }
        if (($op -ge 0x70 -and $op -le 0x7f) -or ($op -ge 0xe0 -and $op -le 0xe3) -or
            ($op -eq 0x0f -and $raw.Length -eq 4 -and $raw[1] -ge 0x80 -and $raw[1] -le 0x8f)) {
            $pending.Push((Get-RelativeTarget $instruction))
        } elseif ($op -in @(0xc2, 0xc3, 0xca, 0xcb, 0xcf)) {
            throw ('Unexpected return inside a case at {0:X4}.' -f $ip)
        } elseif ($instruction.Text -match '^(J[A-Z]+|LOOP[A-Z]*) ') {
            throw ('Unsupported control transfer at {0:X4}, opcode {1:X2}.' -f $ip, $op)
        }
        $pending.Push($ip + $raw.Length)
    }
    return @{
        Instructions = $seen.Count
        Globals = @($globals.Keys | Sort-Object | ForEach-Object { 'DS:{0:X4}' -f $_ })
        Calls = @($calls.Keys | Sort-Object)
    }
}

$enumText = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $header).ProviderPath)
$block = [regex]::Match($enumText, '(?s)enum class BehaviorType\s*\{(.*?)\};')
if (-not $block.Success) { throw 'Cannot locate the renderer BehaviorType enum.' }
$names = @{}
foreach ($match in [regex]::Matches($block.Groups[1].Value, '(?m)^\s*(\w+)\s*=\s*(\d+)\s*,')) {
    $value = [int]$match.Groups[2].Value
    if ($names.ContainsKey($value)) { throw "Duplicate renderer behavior value $value." }
    $names[$value] = $match.Groups[1].Value
}
if ($names.Count -eq 0 -or $names[14] -ne 'BatIndoor') { throw 'Unexpected renderer behavior definitions.' }
$spine = Trace-Selector 0
if ($spine.Entry -ne 0x5110) { throw 'Behavior zero no longer reaches the common path.' }
$caseValues = @($spine.Comparisons | Sort-Object -Unique)
if ($caseValues.Count -ne 187 -or $caseValues -notcontains 65535 -or $caseValues -notcontains 300) {
    throw "Unexpected comparison-cascade coverage: $($caseValues.Count) comparisons; values: $($caseValues -join ',')."
}
$rows = [System.Collections.Generic.List[object]]::new()
foreach ($value in @(@($names.Keys) + $caseValues | Sort-Object -Unique)) {
    $trace = Trace-Selector $value
    $name = $names[$value]
    if ($value -eq 65535) { $name = 'DisabledSentinel' }
    elseif (-not $name) { $name = 'Behavior_' + $value }
    $specific = $trace.Entry -ne 0x5110
    $references = @{ Instructions = 0; Globals = @(); Calls = @() }
    if ($specific) { $references = Get-CaseReferences $trace.Entry }
    $rows.Add([ordered]@{
        value = $value
        name = $name
        namedInRenderer = $names.ContainsKey($value)
        hasSpecificCase = $specific
        entry = '0000:{0:X4}' -f $trace.Entry
        reachableInstructionCount = $references.Instructions
        directGlobals = @($references.Globals)
        directCalls = @($references.Calls)
    })
}
if (($rows | Where-Object hasSpecificCase).Count -ne 187) {
    throw 'Not all binary cases were represented.'
}
$bat = $rows | Where-Object value -eq 14
if ($bat.entry -ne '0000:161D' -or $bat.directCalls -notcontains '10AC:3391') {
    throw 'BatIndoor anchor verification failed.'
}

$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$paths = @('GameEnums.h', 'BehaviorCases.inc', 'BehaviorHandlers.cpp', 'BehaviorMap.json')
foreach ($file in $paths) {
    if (Test-Path -LiteralPath (Join-Path $output $file)) {
        throw "Output already exists: $file. Generate to a new directory to preserve recovered implementations."
    }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$catalog = [System.Collections.Generic.List[string]]::new()
$catalog.Add('// Generated from the original executable and renderer enum; see GenerateBehaviorScaffold.ps1.')
$handlers = [System.Collections.Generic.List[string]]::new()
$handlers.Add('// Initial unrecovered handlers. Edit these bodies as behavior is recovered; do not overwrite them by regenerating.')
$handlers.Add('#include "BehaviorDispatch.h"')
$handlers.Add('')
$handlers.Add('namespace Desperabis')
$handlers.Add('{')
$gameEnums = [System.Collections.Generic.List[string]]::new()
$gameEnums.Add('// Generated from renderer enums and original executable cases; see GenerateBehaviorScaffold.ps1.')
$gameEnums.Add('#pragma once')
$gameEnums.Add('')
$gameEnums.Add('#include <cstdint>')
$gameEnums.Add('')
$gameEnums.Add('namespace Desperabis')
$gameEnums.Add('{')
$enumTypes = [ordered]@{
    BehaviorType = 'std::uint16_t'
    CollisionType = 'std::uint8_t'
    DisplayType = 'std::uint8_t'
    WeaponType = 'std::uint8_t'
}
foreach ($enumName in $enumTypes.Keys) {
    $enumBlock = [regex]::Match($enumText, "(?s)enum class $enumName\s*\{(.*?)\};")
    if (-not $enumBlock.Success) { throw "Cannot locate renderer enum $enumName." }
    $gameEnums.Add(('    enum class {0} : {1}' -f $enumName, $enumTypes[$enumName]))
    $gameEnums.Add('    {')
    $gameEnums.Add($enumBlock.Groups[1].Value.TrimEnd())
    if ($enumName -eq 'BehaviorType') {
        $gameEnums.Add('')
        $gameEnums.Add('        // Additional selectors observed in the original 16-bit dispatcher.')
        foreach ($row in $rows | Where-Object { -not $_.namedInRenderer }) {
            $gameEnums.Add(('        {0} = {1},' -f $row.name, $row.value))
        }
    }
    $gameEnums.Add('    };')
    $gameEnums.Add('')
}
$gameEnums.Add('    static_assert(sizeof(BehaviorType) == 2);')
$gameEnums.Add('    static_assert(sizeof(CollisionType) == 1);')
$gameEnums.Add('    static_assert(sizeof(DisplayType) == 1);')
$gameEnums.Add('    static_assert(sizeof(WeaponType) == 1);')
$gameEnums.Add('}')
foreach ($row in $rows) {
    $offset = [Convert]::ToInt32($row.entry.Split(':')[1], 16)
    if ($row.hasSpecificCase) {
        $catalog.Add(('BEHAVIOR_CASE({0}, 0x{1:X4})' -f $row.name, $offset))
        $handlers.Add(('    DispatchResult Function_{0}(RawGameObject&, GameContext&)' -f $row.name))
        $handlers.Add('    {')
        $handlers.Add(('        return {{ DispatchStatus::UnimplementedHandler, BehaviorType::{0}, 0x{1:X4} }};' -f $row.name, $offset))
        $handlers.Add('    }')
        $handlers.Add('')
    } else {
        $catalog.Add(('BEHAVIOR_COMMON({0})' -f $row.name))
    }
}
$handlers.Add('}')
$report = [ordered]@{
    schemaVersion = 1
    executableSha256 = $expectedHash
    rendererHeaderSha256 = (Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash.ToLowerInvariant()
    functionEntry = '0000:1094'
    selectorLoad = '0000:114D'
    commonPostprocessing = '0000:5110'
    specificCaseCount = 186
    sentinelCaseCount = 1
    referenceScope = 'Reachable instructions inside each case, stopping before common postprocessing; callees not followed. Direct absolute DS operands only.'
    cases = $rows.ToArray()
}
$utf8 = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText((Join-Path $output 'GameEnums.h'), ($gameEnums -join "`n") + "`n", $utf8)
[System.IO.File]::WriteAllText((Join-Path $output 'BehaviorCases.inc'), ($catalog -join "`n") + "`n", $utf8)
[System.IO.File]::WriteAllText((Join-Path $output 'BehaviorHandlers.cpp'), ($handlers -join "`n") + "`n", $utf8)
[System.IO.File]::WriteAllText((Join-Path $output 'BehaviorMap.json'), ($report | ConvertTo-Json -Depth 8) + "`n", $utf8)
Write-Host "Generated $($rows.Count) catalog entries, 186 behavior handlers, and one sentinel handler in $output"
