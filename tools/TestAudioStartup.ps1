$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskOut=Join-Path $taskRoot 'work\audio-startup-tests'
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
$taskCompiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
& $taskCompiler /nologo /target:exe /platform:x86 /reference:System.dll /reference:System.Core.dll /reference:System.Xml.dll "/out:$taskOut\AudioStartupTests.exe" (Join-Path $taskRoot 'launcher\GameCompatibility.cs') (Join-Path $taskRoot 'launcher\LauncherCore.cs') (Join-Path $taskRoot 'launcher\AudioStartup.cs') (Join-Path $taskRoot 'tests\AudioStartupTests.cs')
if($LASTEXITCODE){throw 'Audio test compilation failed.'}
& (Join-Path $taskOut 'AudioStartupTests.exe')
if($LASTEXITCODE){throw 'Audio startup tests failed.'}
