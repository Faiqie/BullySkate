@echo off
setlocal
if not defined BULLY_VCVARS for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BULLY_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
call "%BULLY_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist work\plugin-build mkdir work\plugin-build
cl /nologo /c /std:c17 /MD /O2 /Gy /DLUA_NUMBER=float /Ivendor\derpys-script-loader\include /Ivendor\SDL3\include /Fowork\plugin-build\ native\plugin.c native\bridge.c
if errorlevel 1 exit /b 1
link /nologo /DLL /OPT:REF /NODEFAULTLIB:LIBCMT /OUT:runtime\BullySkate.asi /LIBPATH:vendor\derpys-script-loader\lib work\plugin-build\plugin.obj work\plugin-build\bridge.obj "%BULLY_MOTION_LIB%" luacore.lib luastd.lib d3d9.lib user32.lib xinput.lib userenv.lib bcrypt.lib ntdll.lib ws2_32.lib advapi32.lib legacy_stdio_definitions.lib
exit /b %errorlevel%
