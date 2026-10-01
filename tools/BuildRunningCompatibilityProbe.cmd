@echo off
setlocal
if not defined BULLY_VCVARS for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BULLY_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%BULLY_VCVARS%" exit /b 1
call "%BULLY_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
cd /d "%BULLY_RUNNING_TEST%"
cl /nologo /O2 /MD /GS- /I"%BULLY_RUNNING_SOURCE%\native" /I"%BULLY_RUNNING_TEST%" /Fe:Bully.exe "%BULLY_RUNNING_SOURCE%\tests\RunningCompatibilityProbe.c" /link user32.lib /FIXED /DYNAMICBASE:NO /BASE:0x400000 /INCREMENTAL:NO >native-build.log 2>&1
exit /b %errorlevel%
