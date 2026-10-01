param([string]$Compiler='', [string]$Python='python')
$ErrorActionPreference='Stop'
$taskPackage=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskChat=$taskPackage
if(!$Compiler){$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
if(!(Test-Path -LiteralPath $Compiler)){throw 'The .NET Framework C# compiler is required.'}
$taskPayload=Join-Path $taskChat 'work\launcher-payload.zip'
& (Join-Path $PSScriptRoot 'MakeDeploymentManifest.ps1')
& $Python (Join-Path $PSScriptRoot 'MakeLauncherPayload.py') $taskPackage $taskPayload
if($LASTEXITCODE){throw 'Payload packaging failed.'}
& $Compiler /nologo /target:exe /platform:x86 /optimize+ "/out:$taskPackage\BullySkateLauncher.exe" "/win32manifest:$taskPackage\launcher\app.manifest" /reference:System.dll /reference:System.Core.dll /reference:System.Xml.dll /reference:System.Windows.Forms.dll /reference:System.IO.Compression.dll "/resource:$taskPayload,BullySkate.payload" (Join-Path $taskPackage 'launcher\LauncherCore.cs') (Join-Path $taskPackage 'launcher\ConsoleLauncher.cs')
if($LASTEXITCODE){throw 'Launcher build failed.'}
Write-Output "Launcher ready: $taskPackage\BullySkateLauncher.exe"
