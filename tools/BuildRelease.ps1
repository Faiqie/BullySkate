param([string]$Python='python',[string]$Version='0.1.0')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $PSScriptRoot 'BuildActions.ps1') -Python $Python
& (Join-Path $PSScriptRoot 'Build.ps1') -Python $Python
& (Join-Path $PSScriptRoot 'TestNativeCompatibilityGuard.ps1')
& (Join-Path $PSScriptRoot 'BuildWorker.ps1')
& (Join-Path $PSScriptRoot 'BuildExtractor.ps1') -Python $Python
& (Join-Path $PSScriptRoot 'BuildLauncher.ps1') -Python $Python
& (Join-Path $PSScriptRoot 'TestLauncherDisplay.ps1')
& (Join-Path $PSScriptRoot 'TestSteamLaunch.ps1')
& (Join-Path $PSScriptRoot 'TestGameCompatibility.ps1')
& (Join-Path $PSScriptRoot 'TestRunningCompatibility.ps1') -Python $Python
Push-Location $taskRoot
try {
 & $Python -m unittest discover -s tests -p 'test_*.py'
 if($LASTEXITCODE){throw 'Packaging tests failed.'}
 & cargo test --lib --locked
 if($LASTEXITCODE){throw 'Input normalization tests failed.'}
 & (Join-Path $PSScriptRoot 'PackageRelease.ps1') -Python $Python -Version $Version
} finally {Pop-Location}
