[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projects = Join-Path $PSScriptRoot 'projects'
$report = Join-Path $PSScriptRoot 'initial-import.txt'
if (Test-Path -LiteralPath (Join-Path $projects 'Desperabis.gpr')) {
    throw 'The Desperabis project already exists. Open it instead of overwriting the analysis.'
}
if (Test-Path -LiteralPath $report) {
    throw 'The import report already exists. Preserve it before repeating the initial import.'
}
New-Item -ItemType Directory -Path $projects -Force | Out-Null
$arguments = @(
    $projects, 'Desperabis',
    '-import', (Join-Path $PSScriptRoot '..\..\Game\Original\DESPERAB.EXE'),
    '-noanalysis',
    '-scriptPath', (Join-Path $PSScriptRoot 'ghidra_scripts'),
    '-postScript', 'VerifyDosImport.java', $report,
    '-log', (Join-Path $projects 'initial-import.log'),
    '-scriptlog', (Join-Path $projects 'initial-import-script.log')
)
& (Join-Path $PSScriptRoot 'StartGhidra.ps1') -Headless -HeadlessArguments $arguments
if (-not $?) {
    throw 'The initial Ghidra import failed.'
}
if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {
    throw 'Ghidra did not produce the import verification report. Inspect the project logs.'
}
if ((Get-Content -LiteralPath $report | Select-Object -Last 1) -ne 'Verification: PASS') {
    throw 'The import report is incomplete. Inspect the project logs.'
}
