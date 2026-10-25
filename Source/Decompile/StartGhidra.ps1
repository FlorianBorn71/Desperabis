[CmdletBinding()]
param(
    [switch]$Headless,
    [string[]]$HeadlessArguments = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ghidraHome = Join-Path $PSScriptRoot 'tools\ghidra_12.2_PUBLIC'
$jdkHome = Join-Path $PSScriptRoot 'tools\jdk-25.0.4.1+1'
$launcher = Join-Path $ghidraHome 'ghidraRun.bat'
if ($Headless) {
    $launcher = Join-Path $ghidraHome 'support\analyzeHeadless.bat'
} elseif ($HeadlessArguments.Count -gt 0) {
    throw 'HeadlessArguments requires -Headless.'
}
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $jdkHome 'bin\java.exe') -PathType Leaf)) {
    throw 'The documented portable Ghidra/JDK installations are missing from tools.'
}

$oldJavaHome = $env:JAVA_HOME
$oldJdkHome = $env:GHIDRA_JAVA_HOME
try {
    $env:JAVA_HOME = $jdkHome
    $env:GHIDRA_JAVA_HOME = $jdkHome
    & $launcher @HeadlessArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Ghidra exited with code $LASTEXITCODE."
    }
} finally {
    $env:JAVA_HOME = $oldJavaHome
    $env:GHIDRA_JAVA_HOME = $oldJdkHome
}
