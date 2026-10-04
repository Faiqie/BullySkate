param([Parameter(Mandatory=$true)][string]$GamePath)
$ErrorActionPreference='Stop'
function Get-DslVersion([string]$Path){
 if(!(Test-Path -LiteralPath $Path -PathType Leaf)){return $null}
 $text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Path))
 if($text -match "derpy's script loader: version (\d+(?:\.\d+)?)"){return [decimal]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)}
 return $null
}
$taskGame=[IO.Path]::GetFullPath($GamePath).TrimEnd('\')
$taskDll=Join-Path $taskGame 'derpy_script_loader.asi'
$taskVersion=Get-DslVersion $taskDll
if($taskVersion -ge 15.3){Write-Output "Keeping your DSL $taskVersion and its settings.";return}
if((Test-Path -LiteralPath $taskDll) -and $null -eq $taskVersion){throw 'The existing script loader was preserved because its version could not be identified. Install the official DSL 15.3 from https://www.nexusmods.com/bullyscholarshipedition/mods/43 then reopen the launcher.'}
foreach($taskProcess in @(Get-Process Bully -ErrorAction SilentlyContinue)){
 if($taskProcess.Path -eq (Join-Path $taskGame 'Bully.exe')){throw 'Close Bully before updating DSL.'}
}
$taskRoot=Join-Path $env:LOCALAPPDATA 'BullySkate\downloads\dsl-15.3'
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$taskZip=Join-Path $taskRoot 'official.zip'
$taskExpected='26e95c71e8dd963eac9f8f9e622d4035f0b974474eb2e6ada9dda26f611140e0'
if(!(Test-Path -LiteralPath $taskZip) -or (Get-FileHash -LiteralPath $taskZip).Hash.ToLowerInvariant() -ne $taskExpected){
 Write-Output 'Downloading official DSL 15.3 from its author...'
 [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
 $taskClient=[Net.WebClient]::new()
 try{
  $taskPage=$taskClient.DownloadString('https://www.mediafire.com/file/9pr0obrjynrbdiu/derpy_script_loader_15.3.zip/file')
  $taskAnchor=[regex]::Match($taskPage,'<a\b[^>]*\bid="downloadButton"[^>]*>',[Text.RegularExpressions.RegexOptions]::IgnoreCase).Value
  $taskUrl=[regex]::Match($taskAnchor,'\bhref="([^"]+)"').Groups[1].Value
  $taskUrl=[Net.WebUtility]::HtmlDecode($taskUrl)
  if(!$taskUrl -or ([uri]$taskUrl).Scheme -ne 'https' -or ([uri]$taskUrl).Host -notmatch '^download[0-9]*\.mediafire\.com$'){throw 'The official DSL download link was unavailable.'}
  $taskClient.DownloadFile($taskUrl,$taskZip)
 }catch{throw 'Could not download the official loader. Install DSL 15.3 from https://www.nexusmods.com/bullyscholarshipedition/mods/43 and reopen this launcher.'}finally{$taskClient.Dispose()}
}
if((Get-FileHash -LiteralPath $taskZip).Hash.ToLowerInvariant() -ne $taskExpected){throw 'The official DSL archive failed verification; your current loader was preserved.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchive=[IO.Compression.ZipFile]::OpenRead($taskZip)
$taskTemp=Join-Path $taskRoot ('verified-'+[Guid]::NewGuid().ToString('N')+'.asi')
try{
 $taskEntry=$taskArchive.GetEntry('derpy_script_loader.asi')
 if(!$taskEntry -or $taskEntry.Length -gt 16000000){throw 'Invalid official DSL archive.'}
 $taskInput=$taskEntry.Open();$taskOutput=[IO.File]::Create($taskTemp)
 try{$taskInput.CopyTo($taskOutput)}finally{$taskOutput.Dispose();$taskInput.Dispose()}
}finally{$taskArchive.Dispose()}
if((Get-DslVersion $taskTemp) -ne 15.3){throw 'The downloaded loader version did not match.'}
if(Test-Path -LiteralPath $taskDll){
 $taskBackup=Join-Path $taskGame ('_derpy_script_loader\motion-backups\dsl-upgrade-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
 New-Item -ItemType Directory -Path $taskBackup -Force | Out-Null
 Copy-Item -LiteralPath $taskDll -Destination (Join-Path $taskBackup 'derpy_script_loader.asi')
}
Copy-Item -LiteralPath $taskTemp -Destination $taskDll -Force
Write-Output 'Official DSL 15.3 installed. Other mods and their settings were preserved.'
