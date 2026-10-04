param([string]$LoaderPath='', [string]$TargetDirectory='', [string]$Python='python')
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
foreach($name in @('BULLY_MOTION_LOADER','BULLY_MOTION_LIB','BULLY_MOTION_OUTPUT','BULLY_SDL_INCLUDE')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
Push-Location $chatRoot
try{
 & $Python (Join-Path $PSScriptRoot 'GenerateNativeProfile.py')
 if($LASTEXITCODE){throw 'Native fingerprint projection failed.'}
 & cargo build --manifest-path (Join-Path $packageRoot 'Cargo.toml') --lib --release --target i686-pc-windows-msvc --target-dir $taskTarget
 if($LASTEXITCODE){throw 'Rust build failed.'}
 $env:BULLY_MOTION_LOADER=$taskLoader
 $env:BULLY_MOTION_LIB=$taskLibrary
 $env:BULLY_MOTION_OUTPUT=Join-Path $packageRoot 'runtime\BullySkate.asi'
 $env:BULLY_SDL_INCLUDE=Join-Path $packageRoot 'vendor\SDL3\include'
 & (Join-Path $PSScriptRoot 'BuildPlugin.cmd')
 if($LASTEXITCODE){throw 'Native skating plugin build failed.'}
 Write-Output "Build ready: $env:BULLY_MOTION_OUTPUT"
}finally{
 foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
 Pop-Location
}
