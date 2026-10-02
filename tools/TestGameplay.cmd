@echo off
setlocal
for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "TASK_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
call "%TASK_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist work mkdir work
cl /nologo /O2 /MT /DLUA_NUMBER=float /Ivendor\derpys-script-loader\include /Inative tests\gameplay_input_tests.c /Fework\gameplay_input_tests.exe /Fowork\gameplay_input_tests.obj
if errorlevel 1 exit /b 1
work\gameplay_input_tests.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /Ivendor\derpys-script-loader\include tests\lua_runner.c /Fework\lua_runner.exe /Fowork\lua_runner.obj /link /LIBPATH:vendor\derpys-script-loader\lib luacore.lib luastd.lib
if errorlevel 1 exit /b 1
work\lua_runner.exe tests\gameplay.lua
exit /b %errorlevel%
