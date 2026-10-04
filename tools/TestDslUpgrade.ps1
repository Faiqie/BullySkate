param([Parameter(Mandatory=$true)][string]$OfficialLoader)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskCases=Join-Path $taskRoot ('work\dsl-tests-'+[Guid]::NewGuid().ToString('N'))
foreach($taskCase in @('current','older','newer','unknown')){
 $taskGame=Join-Path $taskCases $taskCase
 $taskOther=Join-Path $taskGame '_derpy_script_loader\scripts\OtherMod'
 New-Item -ItemType Directory -Path $taskOther -Force | Out-Null
 [IO.File]::WriteAllText((Join-Path $taskOther 'config.ini'),"main_script: main.lua`n# custom user setting`n")
 [IO.File]::WriteAllText((Join-Path $taskOther 'config.txt'),"custom old configuration`n")
 [IO.File]::WriteAllText((Join-Path $taskOther 'main.lua'),"function main() while true do Wait(0) end end`n")
 $taskGlobal=Join-Path $taskGame '_derpy_script_loader\dslconfig.ini'
 [IO.File]::WriteAllText($taskGlobal,"config_version6`nallow_system_access: false`nallow_networking: false`n")
 $taskDll=Join-Path $taskGame 'derpy_script_loader.asi'
 if($taskCase -eq 'current'){Copy-Item -LiteralPath $OfficialLoader -Destination $taskDll}
 elseif($taskCase -eq 'unknown'){[IO.File]::WriteAllText($taskDll,'unidentified loader')}
 else{[IO.File]::WriteAllText($taskDll,"derpy's script loader: version "+$(if($taskCase -eq 'older'){'9'}else{'16.0'}))}
 $taskBefore=@{};Get-ChildItem -LiteralPath $taskGame -Recurse -File | ForEach-Object {$taskBefore[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash}
 $taskRejected=$false
 try{& (Join-Path $PSScriptRoot 'EnsureDsl.ps1') -GamePath $taskGame}catch{if($taskCase -ne 'unknown'){throw};$taskRejected=$true}
 if($taskCase -eq 'unknown' -and !$taskRejected){throw 'An unidentified loader was replaced.'}
 foreach($taskFile in $taskBefore.Keys){
  if($taskCase -eq 'older' -and $taskFile -eq $taskDll){continue}
  if((Get-FileHash -LiteralPath $taskFile).Hash -ne $taskBefore[$taskFile]){throw "User file changed: $taskFile"}
 }
 if($taskCase -eq 'older'){
  if((Get-FileHash -LiteralPath $taskDll).Hash -ne (Get-FileHash -LiteralPath $OfficialLoader).Hash){throw 'Upgrade was not the verified official loader.'}
  $taskBackup=Get-ChildItem -LiteralPath (Join-Path $taskGame '_derpy_script_loader\motion-backups') -Recurse -Filter 'derpy_script_loader.asi' -File
  if(!$taskBackup -or (Get-FileHash -LiteralPath $taskBackup[0].FullName).Hash -ne $taskBefore[$taskDll]){throw 'Previous loader was not backed up.'}
 }
}
Write-Output 'PASS: official current/newer loaders preserved, old loader backed up and upgraded, unknown loader untouched; other mods and global settings unchanged.'
