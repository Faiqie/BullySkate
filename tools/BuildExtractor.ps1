param([string]$Python='python')
$ErrorActionPreference='Stop'
$taskPackage=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskChat=$taskPackage
$taskNames=Join-Path $taskPackage 'extractor\tools\asset_pipeline\names.txt'
$taskPyffi=Join-Path $taskPackage 'vendor\python\pyffi'
& $Python -m PyInstaller --noconfirm --clean --onefile --console --name SkateAssetExtractor --distpath (Join-Path $taskPackage 'runtime') --workpath (Join-Path $taskChat 'work\extractor-build') --specpath (Join-Path $taskChat 'work') --paths (Join-Path $taskPackage 'vendor\python') --hidden-import bully_geometry --hidden-import bully_rails --hidden-import bully_bind --hidden-import pyffi.formats.nif --add-data ((Join-Path $taskPyffi 'VERSION')+';pyffi') --add-data ((Join-Path $taskPyffi 'formats')+';pyffi/formats') --add-data ($taskNames+';tools/asset_pipeline') (Join-Path $taskPackage 'extractor\extract.py')
if($LASTEXITCODE){throw 'Extractor build failed.'}
