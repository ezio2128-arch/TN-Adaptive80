[CmdletBinding()]
param([switch]$DeleteProfiles)
$ErrorActionPreference='Stop'
# Stop only processes whose executable belongs to this package.
$packages=@(Get-AppxPackage -Name FramePulse)
foreach ($package in $packages) {
    Get-Process FramePulseCore -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.Path -and $_.Path.StartsWith($package.InstallLocation+'\',[StringComparison]::OrdinalIgnoreCase)) { Stop-Process -Id $_.Id }
    }
    Remove-AppxPackage -Package $package.PackageFullName
}
if (!$DeleteProfiles) {
    $answer=Read-Host 'Conservar perfiles y logs? [S/n]'
    $DeleteProfiles=$answer -match '^(n|no)$'
}
if ($DeleteProfiles) {
    $data=Join-Path $env:LOCALAPPDATA 'FramePulse'
    if (Test-Path $data) { Remove-Item $data -Recurse -Force }
}
Write-Host 'Uninstalled. Close any game with Advanced Pacing attached to release its dormant DLL.'
