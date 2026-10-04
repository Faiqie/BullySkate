$ErrorActionPreference='Stop'
$taskRoot=Join-Path $env:LOCALAPPDATA 'BullySkate\downloads\directx-june2010'
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$taskRedist=Join-Path $taskRoot 'directx_Jun2010_redist.exe'
$taskHash='053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b'
if(!(Test-Path -LiteralPath $taskRedist) -or (Get-FileHash -LiteralPath $taskRedist).Hash.ToLowerInvariant() -ne $taskHash){
 Write-Output 'Downloading the official Microsoft legacy DirectX runtime...'
 [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
 $taskClient=[Net.WebClient]::new()
 try{$taskClient.DownloadFile('https://download.microsoft.com/download/8/4/a/84a35bf1-dafe-4ae8-82af-ad2ae20b6b14/directx_Jun2010_redist.exe',$taskRedist)}finally{$taskClient.Dispose()}
}
$taskSignature=Get-AuthenticodeSignature -FilePath $taskRedist
if((Get-FileHash -LiteralPath $taskRedist).Hash.ToLowerInvariant() -ne $taskHash -or $taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation'){throw 'Microsoft runtime verification failed. Nothing was installed.'}
$taskExtract=Join-Path $taskRoot ('setup-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskExtract | Out-Null
$taskUnpack=Start-Process -FilePath $taskRedist -ArgumentList @('/Q',('/T:"'+$taskExtract+'"')) -PassThru -Wait -WindowStyle Hidden
if($taskUnpack.ExitCode){throw 'Could not extract the official Microsoft installer.'}
$taskSetup=Join-Path $taskExtract 'DXSETUP.exe'
$taskSetupSignature=Get-AuthenticodeSignature -FilePath $taskSetup
if($taskSetupSignature.Status -ne 'Valid' -or $taskSetupSignature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation'){throw 'The extracted Microsoft installer failed signature verification.'}
Write-Output 'Installing Microsoft legacy audio components. Accept the Windows administrator prompt to continue.'
$taskInstall=Start-Process -FilePath $taskSetup -ArgumentList '/silent' -Verb RunAs -PassThru -Wait -WindowStyle Hidden
if($taskInstall.ExitCode){throw ('Microsoft DirectX setup failed with exit code '+$taskInstall.ExitCode+'.')}
Write-Output 'Microsoft DirectX repair completed.'
