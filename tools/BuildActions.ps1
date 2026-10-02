param([string]$Python='python')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& $Python (Join-Path $PSScriptRoot 'make_actions.py')
if($LASTEXITCODE){throw 'Action source generation failed.'}
Push-Location $taskRoot
try {
 # Explicit offsets/types in the authored MACT compile without game templates.
 & $Python (Join-Path $PSScriptRoot 'mact\MACT_TO_CAT.py') 'scripts\BullyMotion\BullyMotion.mact'
 if($LASTEXITCODE){throw 'Authored action bank compilation failed.'}
 # The compiler writes to its working directory; update the installed bank too.
 Copy-Item -LiteralPath (Join-Path $taskRoot 'BullyMotion.cat') -Destination (Join-Path $taskRoot 'scripts\BullyMotion\BullyMotion.cat') -Force
} finally {Pop-Location}
