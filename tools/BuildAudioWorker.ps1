param([string]$TargetDirectory='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$TargetDirectory){$TargetDirectory=Join-Path $taskRoot 'work\audio-target'}
& cargo build --manifest-path (Join-Path $taskRoot 'audio-worker\Cargo.toml') --locked --release --target-dir $TargetDirectory
if($LASTEXITCODE){throw 'Skate 3 sound worker build failed.'}
Copy-Item -LiteralPath (Join-Path $TargetDirectory 'release\SkateAudioWorker.exe') -Destination (Join-Path $taskRoot 'runtime\SkateAudioWorker.exe') -Force
