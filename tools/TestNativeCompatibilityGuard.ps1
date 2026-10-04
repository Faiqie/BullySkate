param([string]$Compiler='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$Compiler){$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
$taskTests=Join-Path $taskRoot 'work\native-compatibility-tests'
New-Item -ItemType Directory -Path $taskTests -Force | Out-Null
& $Compiler /nologo /target:exe /platform:x86 "/out:$taskTests\NativeCompatibilityGuardTests.exe" (Join-Path $taskRoot 'tests\NativeCompatibilityGuardTests.cs')
if($LASTEXITCODE){throw 'Native guard probe build failed.'}
& (Join-Path $taskTests 'NativeCompatibilityGuardTests.exe') (Join-Path $taskRoot 'runtime\BullySkate.asi')
if($LASTEXITCODE){throw 'Native compatibility guard test failed.'}
