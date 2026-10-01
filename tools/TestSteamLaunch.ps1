param([string]$Compiler='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$Compiler){$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
$taskTests=Join-Path $taskRoot ('work\steam-launch-tests-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTests -Force | Out-Null
& $Compiler /nologo /target:exe /platform:x86 "/out:$taskTests\SteamLaunchTests.exe" /reference:System.dll /reference:System.Core.dll /reference:System.Xml.dll (Join-Path $taskRoot 'launcher\GameCompatibility.cs') (Join-Path $taskRoot 'launcher\LauncherCore.cs') (Join-Path $taskRoot 'tests\SteamLaunchTests.cs')
if($LASTEXITCODE){throw 'Steam launch test build failed.'}
& (Join-Path $taskTests 'SteamLaunchTests.exe') $taskTests $taskRoot
if($LASTEXITCODE){throw 'Steam launch tests failed.'}
