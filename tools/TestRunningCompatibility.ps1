param([string]$Python='python',[string]$Compiler='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskTest=Join-Path $taskRoot 'work\running-compatibility-tests'
New-Item -ItemType Directory -Path $taskTest -Force | Out-Null
if(!$Compiler){$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
& $Python (Join-Path $PSScriptRoot 'GenerateNativeProfile.py')
if($LASTEXITCODE){throw 'Native test metadata preparation failed.'}
& $Python (Join-Path $PSScriptRoot 'MakeRunningCompatibilityFixture.py') $taskTest
if($LASTEXITCODE){throw 'Native SHA-256 test vectors failed.'}
$taskOldTest=$env:BULLY_RUNNING_TEST;$taskOldSource=$env:BULLY_RUNNING_SOURCE
try {
 $env:BULLY_RUNNING_TEST=$taskTest;$env:BULLY_RUNNING_SOURCE=$taskRoot
 & (Join-Path $PSScriptRoot 'BuildRunningCompatibilityProbe.cmd')
 if($LASTEXITCODE){throw "Native fixture compilation failed. See $taskTest\native-build.log"}
} finally {$env:BULLY_RUNNING_TEST=$taskOldTest;$env:BULLY_RUNNING_SOURCE=$taskOldSource}
& $Python (Join-Path $PSScriptRoot 'MakeRunningCompatibilityFixture.py') $taskTest --profile
if($LASTEXITCODE){throw 'Fixture profile preparation failed.'}
& $Compiler /nologo /target:exe /platform:x86 "/out:$taskTest\RunningCompatibilityTests.exe" /reference:System.dll /reference:System.Core.dll /reference:System.Xml.dll "/resource:$taskTest\fixture.xml,BullySkate.game-layout" (Join-Path $taskRoot 'launcher\GameCompatibility.cs') (Join-Path $taskRoot 'tests\RunningCompatibilityTests.cs')
if($LASTEXITCODE){throw 'Running compatibility tests failed to compile.'}
& (Join-Path $taskTest 'RunningCompatibilityTests.exe') (Join-Path $taskTest 'Bully.exe')
if($LASTEXITCODE){throw 'Running compatibility tests failed.'}
