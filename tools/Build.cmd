@echo off
setlocal
if not defined BULLY_VCVARS for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BULLY_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%BULLY_VCVARS%" exit /b 1
call "%BULLY_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
cd /d "%BULLY_MOTION_LOADER%\src"
cl /nologo /c /std:c17 /MD /O2 /MP4 /DLUA_NUMBER=float /DDSL_DISABLE_SYSTEM_ACCESS /I..\include *.c library\*.c
if errorlevel 1 exit /b 1
cd client
cl /nologo /c /std:c17 /MD /O2 /MP4 /DLUA_NUMBER=float /DDSL_DISABLE_SYSTEM_ACCESS /I..\..\include *.c *.cpp library\*.c
if errorlevel 1 exit /b 1
link /nologo /DLL /NODEFAULTLIB:LIBCMT /OUT:"%BULLY_MOTION_OUTPUT%" /MAP:"%BULLY_MOTION_LIB%.map" /LIBPATH:..\..\lib *.obj ..\*.obj "%BULLY_MOTION_LIB%" luacore.lib luastd.lib libpng.lib bz2.lib lzma.lib zip.lib zlib.lib zstd.lib d3d9.lib dinput8.lib dwrite.lib advapi32.lib gdi32.lib user32.lib ws2_32.lib xinput.lib userenv.lib bcrypt.lib ntdll.lib legacy_stdio_definitions.lib
exit /b %errorlevel%
