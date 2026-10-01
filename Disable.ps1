param([Parameter(Mandatory=$true)][string]$GamePath)
$ErrorActionPreference='Stop'
$taskGame=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
$exe=Join-Path $taskGame 'Bully.exe'
if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Bully.exe was not found.'}
foreach($process in @(Get-Process Bully -ErrorAction SilentlyContinue)){
 if($process.Path -eq $exe){throw 'Close Bully before disabling the collection.'}
}
$scriptRoot=[IO.Path]::GetFullPath((Join-Path $taskGame '_derpy_script_loader\scripts'))
$source=[IO.Path]::GetFullPath((Join-Path $scriptRoot 'BullyMotion'))
if(!($source.StartsWith($scriptRoot+'\',[StringComparison]::OrdinalIgnoreCase))){throw 'Invalid collection path.'}
if(!(Test-Path -LiteralPath $source)){Write-Output 'Bully Motion is already disabled.';exit}
$disabled=[IO.Path]::GetFullPath((Join-Path $taskGame ('_derpy_script_loader\motion-disabled\BullyMotion-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))))
if(!($disabled.StartsWith($taskGame+'\',[StringComparison]::OrdinalIgnoreCase))){throw 'Invalid disabled destination.'}
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($disabled)) -Force | Out-Null
Move-Item -LiteralPath $source -Destination $disabled
Write-Output "Bully Motion disabled. Recoverable files: $disabled"
