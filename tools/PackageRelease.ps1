param([string]$Version='0.1.0-beta.3',[string]$Python='python')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& $Python (Join-Path $PSScriptRoot 'AuditDistribution.py')
if($LASTEXITCODE){throw 'Distribution audit failed.'}
$taskRelease=Join-Path $taskRoot 'release'
New-Item -ItemType Directory -Path $taskRelease -Force | Out-Null
$taskStage=Join-Path $taskRoot ('work\release-stage-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskStage -Force | Out-Null
foreach($taskName in @('BullySkateLauncher.exe','BullySkateLauncher.exe.config','README.md','THIRD_PARTY_NOTICES.md','LICENSE','Setup.cmd','Verify.cmd','CheckBully.cmd','Disable.cmd','Disable.ps1','release-validation.json')){
 Copy-Item -LiteralPath (Join-Path $taskRoot $taskName) -Destination $taskStage
}
foreach($taskName in @('docs','licenses')){Copy-Item -LiteralPath (Join-Path $taskRoot $taskName) -Destination $taskStage -Recurse}
New-Item -ItemType Directory -Path (Join-Path $taskStage 'tools') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'DisableSaved.ps1') -Destination (Join-Path $taskStage 'tools')
$taskStagePrefix=$taskStage.TrimEnd('\')+'\'
$taskLines=@(Get-ChildItem -LiteralPath $taskStage -Recurse -File | Sort-Object FullName | ForEach-Object {
 $taskHash=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
 $taskHash+'  '+$_.FullName.Substring($taskStagePrefix.Length).Replace('\','/')
})
[IO.File]::WriteAllLines((Join-Path $taskStage 'SHA256SUMS.txt'),$taskLines,[Text.ASCIIEncoding]::new())
$taskZip=Join-Path $taskRelease ("BullySkate-Windows-v$Version.zip")
Compress-Archive -LiteralPath @(Get-ChildItem -LiteralPath $taskStage | ForEach-Object {$_.FullName}) -DestinationPath $taskZip -Force
# Remove the obsolete standalone downloads from earlier packaging runs.
foreach($taskName in @('BullySkateLauncher.exe','SHA256SUMS.txt')){
 $taskOldArtifact=[IO.Path]::GetFullPath((Join-Path $taskRelease $taskName))
 if([IO.Path]::GetDirectoryName($taskOldArtifact) -ne $taskRelease){throw 'Unsafe release artifact path.'}
 if(Test-Path -LiteralPath $taskOldArtifact){Remove-Item -LiteralPath $taskOldArtifact -Force}
}
Write-Output "Release ready: $taskZip"
