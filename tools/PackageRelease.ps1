param([string]$Version='0.1.0-beta.1',[string]$Python='python')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& $Python (Join-Path $PSScriptRoot 'AuditDistribution.py')
if($LASTEXITCODE){throw 'Distribution audit failed.'}
$taskRelease=Join-Path $taskRoot 'release'
New-Item -ItemType Directory -Path $taskRelease -Force | Out-Null
$taskStage=Join-Path $taskRoot ('work\release-stage-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskStage -Force | Out-Null
foreach($taskName in @('BullySkateLauncher.exe','BullySkateLauncher.exe.config','README.md','THIRD_PARTY_NOTICES.md','LICENSE','Setup.cmd','Verify.cmd','Disable.cmd','Disable.ps1','release-validation.json')){
 Copy-Item -LiteralPath (Join-Path $taskRoot $taskName) -Destination $taskStage
}
foreach($taskName in @('docs','licenses')){Copy-Item -LiteralPath (Join-Path $taskRoot $taskName) -Destination $taskStage -Recurse}
New-Item -ItemType Directory -Path (Join-Path $taskStage 'tools') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'DisableSaved.ps1') -Destination (Join-Path $taskStage 'tools')
$taskZip=Join-Path $taskRelease ("BullySkate-Windows-v$Version.zip")
Compress-Archive -LiteralPath @(Get-ChildItem -LiteralPath $taskStage | ForEach-Object {$_.FullName}) -DestinationPath $taskZip -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'BullySkateLauncher.exe') -Destination $taskRelease -Force
$taskLines=@(Get-FileHash -LiteralPath $taskZip,(Join-Path $taskRelease 'BullySkateLauncher.exe') -Algorithm SHA256 | ForEach-Object {$_.Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($_.Path)})
[IO.File]::WriteAllLines((Join-Path $taskRelease 'SHA256SUMS.txt'),$taskLines,[Text.ASCIIEncoding]::new())
Write-Output "Release ready: $taskZip"
