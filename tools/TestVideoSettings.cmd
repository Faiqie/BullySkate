@echo off
setlocal
for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BULLY_TEST_VCVARS=%%I\VC\Auxiliary\Build\vcvarsall.bat"
call "%BULLY_TEST_VCVARS%" x86 >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist work\video-tests mkdir work\video-tests
cl /nologo /std:c17 /MD /O2 /Gy /DLUA_NUMBER=float /Ivendor\derpys-script-loader\include /Fowork\video-tests\video.obj /Fework\video-tests\video-tests.exe tests\video_settings_tests.c /link /OPT:REF /LIBPATH:vendor\derpys-script-loader\lib luacore.lib luastd.lib user32.lib
if errorlevel 1 exit /b 1
work\video-tests\video-tests.exe
exit /b %errorlevel%
