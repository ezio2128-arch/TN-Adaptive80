[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release', [string]$CertificateThumbprint)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio 2022 with C++ x64 and UWP C# tools first.' }
$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild not found.' }
$pmon=Join-Path $root 'ThirdParty\PresentMon.exe'
$expected='B2A706BC6AD475749E3B7E3409263AA1E6906D45BDCF993F6DBC0F660188F1AF'
if ((Get-FileHash $pmon -Algorithm SHA256).Hash -ne $expected) { throw 'PresentMon 2.6.0 hash mismatch. Build aborted.' }
$packages=Join-Path $root 'out\packages'
New-Item $packages -ItemType Directory -Force | Out-Null
& $msbuild (Join-Path $root 'FramePulse.sln') /restore /m /p:Configuration=$Configuration /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:AppxBundle=Never /p:AppxPackageSigningEnabled=false /p:UapAppxPackageBuildMode=SideloadOnly "/p:AppxPackageDir=$packages\"
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed. Read MSBuild errors; no package is considered released.' }
& (Join-Path $root "out\bin\$Configuration\FramePulseTests.exe")
if ($LASTEXITCODE -ne 0) { throw 'Adaptive tests failed.' }
$package=Get-ChildItem $packages -Recurse -File | Where-Object { $_.Name -like 'FramePulseWidget*' -and $_.Extension -in '.appx','.msix' -and $_.FullName -notmatch 'Dependencies' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$package) { Get-ChildItem $packages -Recurse -File | ForEach-Object { Write-Host $_.FullName } }
if (!$package) { throw 'UWP package was not produced.' }
$destination=Join-Path $root 'out\FramePulse_1.0.0_candidate.msix'
Copy-Item $package.FullName $destination -Force
if ($CertificateThumbprint) {
    $signtool=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe'
    & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint $destination
    if ($LASTEXITCODE -ne 0) { throw 'Signing failed.' }
    & $signtool verify /pa $destination
    if ($LASTEXITCODE -ne 0) { throw 'Signature verification failed.' }
    Write-Host "Signed validation candidate: $destination"
} else { Write-Host "Unsigned candidate: $destination — sign with a trusted certificate before installing." }
Write-Host 'Release remains blocked until Docs/ACCEPTANCE.md is completed on Windows.'
