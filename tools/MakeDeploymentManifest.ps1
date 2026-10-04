$ErrorActionPreference='Stop'
$taskPackage=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$taskFiles=@(Get-ChildItem -LiteralPath (Join-Path $taskPackage 'runtime'),(Join-Path $taskPackage 'scripts'),(Join-Path $taskPackage 'licenses') -File -Recurse | Where-Object {$_.Extension -notin @('.exp','.lib','.pdb','.obj','.map') -and $_.Name -notin @('derpy_script_loader.asi','config.txt')})
foreach($taskFile in $taskFiles){
 $taskRelative=$taskFile.FullName.Substring($taskPackage.Length+1).Replace('\','/')
 if($taskRelative -match '(^|/)(private|skate-assets|assets)/' -or $taskFile.Extension -in @('.xex','.img','.dir','.nif','.nft','.dds','.abin','.bmgeo','.bmrails','.vlt','.wav','.ogg','.mp3','.grain','.snr','.abk','.bnk','.mxb')){throw "Game data cannot enter the launcher payload: $taskRelative"}
 if($taskRelative -like 'scripts/*' -and $taskFile.Extension -notin @('.lua','.txt','.ini','.cat','.mact') -and $taskRelative -notmatch '^scripts/BullyMotion/ui/(right-stick|dpad-left|dpad-down|dpad-right)\.png$'){throw "Unexpected script payload: $taskRelative"}
}
$taskFiles+=Get-Item -LiteralPath (Join-Path $taskPackage 'Install.ps1')
$taskFiles+=Get-Item -LiteralPath (Join-Path $PSScriptRoot 'EnsureDsl.ps1')
$taskFiles+=Get-Item -LiteralPath (Join-Path $PSScriptRoot 'RepairAudio.ps1')
$taskLines=@($taskFiles | Sort-Object FullName | ForEach-Object {
 $relative=$_.FullName.Substring($taskPackage.Length+1).Replace('\','/')
 (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()+'  '+$relative
})
[IO.File]::WriteAllLines((Join-Path $taskPackage 'deployment.sha256'),$taskLines,[Text.ASCIIEncoding]::new())
Write-Output "Deployment manifest: $($taskLines.Count) files"
