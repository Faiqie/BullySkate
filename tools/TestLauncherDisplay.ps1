param([string]$Compiler='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$Compiler){$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
$taskTestRoot=Join-Path $taskRoot 'work\display-tests'
New-Item -ItemType Directory -Path $taskTestRoot -Force | Out-Null
& $Compiler /nologo /target:exe /platform:x86 "/out:$taskTestRoot\Bully.exe" (Join-Path $taskRoot 'tests\GameDpiProbe.cs')
if($LASTEXITCODE){throw 'DPI probe build failed.'}
& $Compiler /nologo /target:exe /platform:x86 "/out:$taskTestRoot\LauncherDisplayTests.exe" "/win32manifest:$taskRoot\launcher\app.manifest" /reference:System.dll /reference:System.Core.dll /reference:System.Xml.dll /reference:System.Windows.Forms.dll (Join-Path $taskRoot 'launcher\GameCompatibility.cs') (Join-Path $taskRoot 'launcher\LauncherCore.cs') (Join-Path $taskRoot 'tests\LauncherDisplayTests.cs')
if($LASTEXITCODE){throw 'Display test build failed.'}
Copy-Item -LiteralPath (Join-Path $taskRoot 'BullySkateLauncher.exe.config') -Destination (Join-Path $taskTestRoot 'LauncherDisplayTests.exe.config') -Force
& (Join-Path $taskTestRoot 'LauncherDisplayTests.exe') $taskTestRoot
if($LASTEXITCODE){throw 'Display tests failed.'}
