param([string]$Compiler='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$Compiler){$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
$taskTests=Join-Path $taskRoot 'work\compatibility-tests'
New-Item -ItemType Directory -Path $taskTests -Force | Out-Null
& $Compiler /nologo /target:exe /platform:x86 "/out:$taskTests\GameCompatibilityTests.exe" /reference:System.dll /reference:System.Core.dll /reference:System.Xml.dll "/resource:$taskRoot\tests\fixtures\compatibility-layout.xml,BullySkate.game-layout" (Join-Path $taskRoot 'launcher\GameCompatibility.cs') (Join-Path $taskRoot 'tests\GameCompatibilityTests.cs')
if($LASTEXITCODE){throw 'Compatibility tests failed to compile.'}
& (Join-Path $taskTests 'GameCompatibilityTests.exe') $taskTests
if($LASTEXITCODE){throw 'Executable compatibility tests failed.'}
