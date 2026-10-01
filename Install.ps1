param(
 [Parameter(Mandatory=$true)][string]$GamePath,
 [Parameter(Mandatory=$true)][string]$SkateAssets,
 [Parameter(Mandatory=$true)][string]$BullyAssets
)
$ErrorActionPreference='Stop'
$taskGame=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
$taskAssets=[IO.Path]::GetFullPath($SkateAssets).TrimEnd('\')
$taskBullyAssets=[IO.Path]::GetFullPath($BullyAssets).TrimEnd('\')
$taskRequired=@('jimmy-bind.json','vehicle-bounds.txt','world.bmgeo','world.bmrails')
$taskBullyEntries=@(Get-Content -LiteralPath (Join-Path $taskBullyAssets 'asset-manifest.sha256') | ForEach-Object {
 if($_ -notmatch '^([a-fA-F0-9]{64})  (.+)$'){throw 'Invalid locally prepared Bully manifest.'}
 $taskAssetHash=$Matches[1];$taskRelative=$Matches[2]
 if($taskRelative -notin $taskRequired){throw 'Unexpected locally prepared Bully asset.'}
 $taskRequired=@($taskRequired | Where-Object {$_ -ne $taskRelative})
 $taskSource=Join-Path $taskBullyAssets $taskRelative
 if(!(Test-Path -LiteralPath $taskSource -PathType Leaf) -or (Get-FileHash -LiteralPath $taskSource).Hash -ne $taskAssetHash){throw "Invalid local Bully data: $taskRelative"}
 [PSCustomObject]@{Relative=$taskRelative;Source=$taskSource}
})
if($taskRequired.Count){throw 'Incomplete locally prepared Bully data.'}
$exe=Join-Path $taskGame 'Bully.exe'
if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Bully.exe was not found.'}
& (Join-Path $PSScriptRoot 'runtime\BullyBuildCheck.exe') $exe
if($LASTEXITCODE){throw 'The executable does not match the supported Bully native engine layout. Run CheckBully.cmd for details.'}
if(!(Test-Path -LiteralPath $taskAssets -PathType Container)){throw 'The Skate rewrite assets folder was not found.'}
foreach($relative in @('Scripts\Scripts.img','Act\Act.img')){
 if(!(Test-Path -LiteralPath (Join-Path $taskGame $relative) -PathType Leaf)){throw "Select the complete Bully game folder; $relative is missing."}
}
$assetManifest=Join-Path $PSScriptRoot 'runtime\expected-skate-assets.sha256'
$assetEntries=@(Get-Content -LiteralPath $assetManifest | ForEach-Object {
 if($_ -notmatch '^([a-fA-F0-9]{64})  (.+)$'){throw 'Invalid prepared asset manifest.'}
 $assetHash=$Matches[1];$relative=$Matches[2]
 $source=[IO.Path]::GetFullPath((Join-Path $taskAssets $relative))
 if(!$source.StartsWith($taskAssets+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid prepared asset path.'}
 if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "Prepared skating data is missing: $relative"}
 if((Get-FileHash -LiteralPath $source).Hash -ne $assetHash){throw "Prepared skating data does not match this build: $relative"}
 [PSCustomObject]@{Relative=$relative;Source=$source;Hash=$assetHash}
})
$asiLoaderFound=$false
$taskBundledLoader=Join-Path $PSScriptRoot 'runtime\asi-loader\dinput8.dll'
$taskBundledLoaderHash=(Get-FileHash -LiteralPath $taskBundledLoader).Hash
foreach($name in @('dinput8.dll','dsound.dll','version.dll','winmm.dll','d3d9.dll')){
 $candidate=Join-Path $taskGame $name
 if(Test-Path -LiteralPath $candidate){
  if([Diagnostics.FileVersionInfo]::GetVersionInfo($candidate).FileDescription -match 'ASI Loader' -or ($name -eq 'dinput8.dll' -and (Get-FileHash -LiteralPath $candidate).Hash -eq $taskBundledLoaderHash)){$asiLoaderFound=$true}
 }
}
if(!$asiLoaderFound -and (Test-Path -LiteralPath (Join-Path $taskGame 'dinput8.dll'))){throw 'The existing dinput8.dll is not the verified ASI loader. It was preserved; use a Bully folder with its ASI loader configured.'}
foreach($process in @(Get-Process Bully -ErrorAction SilentlyContinue)){
 if($process.Path -eq $exe){throw 'Close Bully before installing.'}
}
$loader=Join-Path $taskGame '_derpy_script_loader'
$scriptRoot=Join-Path $loader 'scripts'
$collection=Join-Path $scriptRoot 'BullyMotion'
$backup=Join-Path $loader ('motion-backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $backup,$scriptRoot -Force | Out-Null
foreach($item in @('derpy_script_loader.asi','_derpy_script_loader\config.txt')){
 $existing=Join-Path $taskGame $item
 if(Test-Path -LiteralPath $existing){Copy-Item -LiteralPath $existing -Destination (Join-Path $backup ([IO.Path]::GetFileName($item))) -Force}
}
if(Test-Path -LiteralPath $collection){Copy-Item -LiteralPath $collection -Destination (Join-Path $backup 'BullyMotion') -Recurse}
if(!(Test-Path -LiteralPath $collection)){
 $disabledRoot=Join-Path $loader 'motion-disabled'
 if(Test-Path -LiteralPath $disabledRoot){
  $previous=Get-ChildItem -LiteralPath $disabledRoot -Directory -Filter 'BullyMotion-*' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if($previous){Copy-Item -LiteralPath $previous.FullName -Destination $collection -Recurse}
 }
}
New-Item -ItemType Directory -Path $collection -Force | Out-Null
# Retire only textures installed by the earlier optional viewmodel. The complete
# old collection is already backed up, and custom collection files are retained.
foreach($name in @('BrownJacket_d.dds','sg_mainmap_d.dds','spudg_d.dds','WP00_lid_d.dds')){
 $obsolete=Join-Path $collection ('assets\'+$name)
 if(Test-Path -LiteralPath $obsolete){
  $retired=Join-Path $backup 'retired-assets'
  New-Item -ItemType Directory -Path $retired -Force | Out-Null
  Move-Item -LiteralPath $obsolete -Destination (Join-Path $retired $name)
 }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\derpy_script_loader.asi') -Destination (Join-Path $taskGame 'derpy_script_loader.asi') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\SkatePhysicsWorker.exe') -Destination (Join-Path $collection 'SkatePhysicsWorker.exe') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\worker-dependencies\vcruntime140.dll') -Destination (Join-Path $collection 'vcruntime140.dll') -Force
$nativeRuntime=Join-Path $taskGame 'vcruntime140.dll'
if(!(Test-Path -LiteralPath $nativeRuntime)){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\native-dependencies\vcruntime140.dll') -Destination $nativeRuntime
}
if(!$asiLoaderFound){Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\asi-loader\dinput8.dll') -Destination (Join-Path $taskGame 'dinput8.dll')}
$openMP=Join-Path $taskGame 'Microsoft.VC80.OpenMP'
New-Item -ItemType Directory -Path $openMP -Force | Out-Null
foreach($dependency in @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'runtime\Microsoft.VC80.OpenMP'))){
 $target=Join-Path $openMP $dependency.Name
 if(!(Test-Path -LiteralPath $target)){Copy-Item -LiteralPath $dependency.FullName -Destination $target}
}
foreach($item in @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'scripts\BullyMotion'))){
 if($item.Name -eq 'settings.dat'){continue}
 if($item.Name -eq 'config.txt' -and (Test-Path -LiteralPath (Join-Path $collection 'config.txt'))){
  $currentConfig=Join-Path $collection 'config.txt'
  $existingLines=@(Get-Content -LiteralPath $currentConfig)
  $existingKeys=@{}
  foreach($line in $existingLines){if($line -match '^\s*([^#\s]+)\s+'){$existingKeys[$Matches[1]]=$true}}
  $additional=@(Get-Content -LiteralPath $item.FullName | Where-Object {$_ -match '^\s*([^#\s]+)\s+' -and !$existingKeys.ContainsKey($Matches[1])})
  if($additional.Count){[IO.File]::WriteAllLines($currentConfig,@($existingLines+$additional),[Text.UTF8Encoding]::new($false))}
  continue
 }
 Copy-Item -LiteralPath $item.FullName -Destination $collection -Recurse -Force
}
$taskNativeAssets=Join-Path $collection 'assets'
New-Item -ItemType Directory -Path $taskNativeAssets -Force | Out-Null
foreach($taskEntry in $taskBullyEntries){Copy-Item -LiteralPath $taskEntry.Source -Destination (Join-Path $taskNativeAssets $taskEntry.Relative) -Force}
Copy-Item -LiteralPath (Join-Path $taskBullyAssets 'asset-manifest.sha256') -Destination (Join-Path $taskNativeAssets 'asset-manifest.sha256') -Force
$installedAssets=Join-Path $collection 'skate-assets'
foreach($entry in $assetEntries){
 $destination=Join-Path $installedAssets $entry.Relative
 New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force | Out-Null
 if(!(Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $destination).Hash -ne $entry.Hash){Copy-Item -LiteralPath $entry.Source -Destination $destination -Force}
}
Copy-Item -LiteralPath $assetManifest -Destination (Join-Path $installedAssets 'asset-manifest.sha256') -Force
[IO.File]::WriteAllText((Join-Path $collection 'source-path.txt'),$installedAssets,[Text.UTF8Encoding]::new($false))
$config=Join-Path $loader 'config.txt'
if(!(Test-Path -LiteralPath $config)){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime\config.txt') -Destination $config
}
Write-Output "Bully Motion installed. Backup: $backup"
Write-Output 'F6 Skate / F8 Edit Skater and FOV / F5 native Bully'
