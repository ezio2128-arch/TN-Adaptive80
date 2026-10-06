[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Package, [string[]]$DependencyPath)
$ErrorActionPreference='Stop'
if (!(Test-Path $Package)) { throw 'MSIX does not exist. Build and sign it first.' }
$sig=Get-AuthenticodeSignature $Package
if ($sig.Status -ne 'Valid') { throw "Package signature is not trusted: $($sig.Status). Follow Docs/INSTALL.md." }
if ($DependencyPath) { Add-AppxPackage -Path $Package -DependencyPath $DependencyPath }
else { Add-AppxPackage -Path $Package }
Write-Host 'Open Start > FramePulse, then Win+G > Widgets > FramePulse.'
Write-Host 'Safe Mode is the default; no game is modified by installation.'
