param([string]$TargetDirectory='')
$ErrorActionPreference='Stop'
$taskPackage=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskChat=$taskPackage
if(!$TargetDirectory){$TargetDirectory=Join-Path $taskChat 'work\motion-target'}
$taskTarget=[IO.Path]::GetFullPath($TargetDirectory)
& cargo build --manifest-path (Join-Path $taskPackage 'Cargo.toml') --bin SkatePhysicsWorker --release --target x86_64-pc-windows-msvc --target-dir $taskTarget
if($LASTEXITCODE){throw 'Worker build failed.'}
Copy-Item -LiteralPath (Join-Path $taskTarget 'x86_64-pc-windows-msvc\release\SkatePhysicsWorker.exe') -Destination (Join-Path $taskPackage 'runtime\SkatePhysicsWorker.exe') -Force
