$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskTests=Join-Path $taskRoot 'work\controller-input-tests'
New-Item -ItemType Directory -Path $taskTests -Force | Out-Null
$taskPrevious=@{}
foreach($taskName in @('BULLY_CONTROLLER_ROOT','BULLY_CONTROLLER_TEST_DIR')){$taskPrevious[$taskName]=[Environment]::GetEnvironmentVariable($taskName,'Process')}
try {
 $env:BULLY_CONTROLLER_ROOT=$taskRoot
 $env:BULLY_CONTROLLER_TEST_DIR=$taskTests
 & (Join-Path $PSScriptRoot 'TestControllerInput.cmd')
 if($LASTEXITCODE){throw 'Controller test compilation failed.'}
 Copy-Item -LiteralPath (Join-Path $taskRoot 'runtime\controller-dependencies\SDL3.dll') -Destination (Join-Path $taskTests 'SDL3.dll') -Force
 & (Join-Path $taskTests 'ControllerInputTests.exe') (Join-Path $taskTests 'SDL3.dll')
 if($LASTEXITCODE){throw 'Controller input tests failed.'}
} finally {foreach($taskName in $taskPrevious.Keys){[Environment]::SetEnvironmentVariable($taskName,$taskPrevious[$taskName],'Process')}}
