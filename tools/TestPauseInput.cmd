@echo off
setlocal
for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BULLY_PAUSE_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
call "%BULLY_PAUSE_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist work\pause-tests mkdir work\pause-tests
cl /nologo /std:c17 /MD /O2 /Gy /DLUA_NUMBER=float /Ivendor\derpys-script-loader\include /Fowork\pause-tests\pause.obj /Fework\pause-tests\pause-tests.exe tests\pause_input_tests.c /link /OPT:REF user32.lib
if errorlevel 1 exit /b 1
work\pause-tests\pause-tests.exe
exit /b %errorlevel%
