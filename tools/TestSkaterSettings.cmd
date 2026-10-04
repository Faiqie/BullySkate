@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
if errorlevel 1 exit /b 1
if not exist work mkdir work
cl /nologo /std:c17 /MT /O2 tests\skater_settings_tests.c /Fework\skater_settings_tests.exe /Fowork\skater_settings_tests.obj
if errorlevel 1 exit /b 1
work\skater_settings_tests.exe
exit /b %errorlevel%
