$ErrorActionPreference='Stop'
$taskSettings=Join-Path $env:LOCALAPPDATA 'BullySkate\console-setup.xml'
if(!(Test-Path -LiteralPath $taskSettings)){throw 'No saved BullySkate setup was found.'}
$taskXml=[Xml.XmlDocument]::new();$taskXml.XmlResolver=$null;$taskXml.Load($taskSettings)
$taskGame=$taskXml.SelectSingleNode('/BullySkate/Game').InnerText
& (Join-Path $PSScriptRoot '..\Disable.ps1') -GamePath $taskGame
