param([string]$LoaderPath='', [string]$TargetDirectory='')
$ErrorActionPreference='Stop'
$packageRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$chatRoot=$packageRoot
if(!$LoaderPath){$LoaderPath=Join-Path $packageRoot 'vendor\derpys-script-loader'}
$taskLoader=[IO.Path]::GetFullPath($LoaderPath)
if(!(Test-Path -LiteralPath (Join-Path $taskLoader 'include\dsl\dsl.h'))){throw 'The prepared DSL source clone is required.'}
if(!$TargetDirectory){$TargetDirectory=Join-Path $chatRoot 'work\motion-target'}
$taskTarget=[IO.Path]::GetFullPath($TargetDirectory)
$taskLibrary=Join-Path $taskTarget 'i686-pc-windows-msvc\release\bully_motion.lib'
$previous=@{}
foreach($name in @('BULLY_MOTION_LOADER','BULLY_MOTION_LIB','BULLY_MOTION_OUTPUT')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
Push-Location $chatRoot
try{
 & cargo build --manifest-path (Join-Path $packageRoot 'Cargo.toml') --lib --release --target i686-pc-windows-msvc --target-dir $taskTarget
 if($LASTEXITCODE){throw 'Rust build failed.'}
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'loader\main.c') -Destination (Join-Path $taskLoader 'src\client\main.c') -Force
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'loader\render.cpp') -Destination (Join-Path $taskLoader 'src\client\render.cpp') -Force
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'loader\dsl.c') -Destination (Join-Path $taskLoader 'src\dsl.c') -Force
 Copy-Item -LiteralPath (Join-Path $packageRoot 'native\bridge.c') -Destination (Join-Path $taskLoader 'src\client\library\lib_fakie.c') -Force
 foreach($name in @('rig.h','viewmodel.h','skate_worker.h','skater_preferences.h','vehicles.h','vehicle_bounds.h','game_compat.h')){
  Copy-Item -LiteralPath (Join-Path $packageRoot ('native\'+$name)) -Destination (Join-Path $taskLoader ('src\client\library\'+$name)) -Force
 }
 $env:BULLY_MOTION_LOADER=$taskLoader
 $env:BULLY_MOTION_LIB=$taskLibrary
 $env:BULLY_MOTION_OUTPUT=Join-Path $packageRoot 'runtime\derpy_script_loader.asi'
 & (Join-Path $PSScriptRoot 'Build.cmd')
 if($LASTEXITCODE){throw 'Native loader build failed.'}
 Write-Output "Build ready: $env:BULLY_MOTION_OUTPUT"
}finally{
 foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
 Pop-Location
}
