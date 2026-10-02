@echo off
setlocal
if not defined BULLY_VCVARS for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BULLY_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%BULLY_VCVARS%" exit /b 1
call "%BULLY_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
pushd "%BULLY_CONTROLLER_TEST_DIR%"
cl /nologo /std:c17 /MT /O2 /W4 /I"%BULLY_CONTROLLER_ROOT%\vendor\SDL3\include" /I"%BULLY_CONTROLLER_ROOT%\native" "%BULLY_CONTROLLER_ROOT%\tests\controller_input_tests.c" /Fe:ControllerInputTests.exe /link "%BULLY_CONTROLLER_ROOT%\vendor\SDL3\lib\x86\SDL3.lib" user32.lib
set "BULLY_CONTROLLER_RESULT=%errorlevel%"
popd
exit /b %BULLY_CONTROLLER_RESULT%
