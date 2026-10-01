$ErrorActionPreference='Stop'
$taskPackage=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\')
$taskFiles=@(Get-ChildItem -LiteralPath (Join-Path $taskPackage 'runtime'),(Join-Path $taskPackage 'scripts'),(Join-Path $taskPackage 'licenses') -File -Recurse)
foreach($taskFile in $taskFiles){
 $taskRelative=$taskFile.FullName.Substring($taskPackage.Length+1).Replace('\','/')
 if($taskRelative -match '(^|/)(private|skate-assets|assets)/' -or $taskFile.Extension -in @('.xex','.img','.dir','.nif','.nft','.dds','.abin','.bmgeo','.bmrails','.vlt')){throw "Game data cannot enter the launcher payload: $taskRelative"}
 if($taskRelative -like 'scripts/*' -and $taskFile.Extension -notin @('.lua','.txt','.cat','.mact')){throw "Unexpected script payload: $taskRelative"}
}
$taskFiles+=Get-Item -LiteralPath (Join-Path $taskPackage 'Install.ps1')
$taskLines=@($taskFiles | Sort-Object FullName | ForEach-Object {
 $relative=$_.FullName.Substring($taskPackage.Length+1).Replace('\','/')
 (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()+'  '+$relative
})
[IO.File]::WriteAllLines((Join-Path $taskPackage 'deployment.sha256'),$taskLines,[Text.ASCIIEncoding]::new())
Write-Output "Deployment manifest: $($taskLines.Count) files"
