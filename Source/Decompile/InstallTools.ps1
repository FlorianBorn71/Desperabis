[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Join-Path $PSScriptRoot 'tools'
New-Item -ItemType Directory -Path $root -Force | Out-Null
$packages = @(
    @{
        Archive = 'OpenJDK25U-jdk_x64_windows_hotspot_25.0.4.1_1.zip'
        Directory = 'jdk-25.0.4.1+1'
        Launcher = 'bin\java.exe'
        Url = 'https://github.com/adoptium/temurin25-binaries/releases/download/jdk-25.0.4.1%2B1/OpenJDK25U-jdk_x64_windows_hotspot_25.0.4.1_1.zip'
        Sha256 = '00c847d804f4a78e9f04f2683faf14fed898535b177b7fc704486cb0284e9283'
    },
    @{
        Archive = 'ghidra_12.2_PUBLIC_20261016.zip'
        Directory = 'ghidra_12.2_PUBLIC'
        Launcher = 'ghidraRun.bat'
        Url = 'https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.2_build/ghidra_12.2_PUBLIC_20261016.zip'
        Sha256 = '2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67'
    }
)

foreach ($package in $packages) {
    $destination = Join-Path $root $package.Directory
    if (Test-Path -LiteralPath $destination) {
        throw "Destination already exists: $destination. Inspect it before reinstalling."
    }
}
foreach ($package in $packages) {
    $archive = Join-Path $root $package.Archive
    Write-Host "Downloading/resuming $($package.Archive)"
    & curl.exe --fail --location --show-error --connect-timeout 20 --retry 3 --continue-at - --output $archive $package.Url
    if ($LASTEXITCODE -ne 0) {
        throw "Download failed: $($package.Archive). The partial archive is retained for resuming."
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $package.Sha256) {
        throw "Checksum mismatch: $archive. Inspect/remove this archive before retrying."
    }
    & tar.exe -xf $archive -C $root
    if ($LASTEXITCODE -ne 0) {
        throw "Extraction failed: $archive. Inspect the partial installation before retrying."
    }
    $launcher = Join-Path (Join-Path $root $package.Directory) $package.Launcher
    if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
        throw "Expected launcher is missing after extraction: $launcher"
    }
    Remove-Item -LiteralPath $archive
    Write-Host "Installed and checksum-verified $($package.Directory)"
}
