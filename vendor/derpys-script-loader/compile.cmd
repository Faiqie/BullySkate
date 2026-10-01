@ECHO OFF
IF NOT EXIST ..\Bully.exe GOTO nobully
IF NOT DEFINED VSCMD_VER GOTO setupvc
:aftervc
FOR /R %%f IN (*.obj) DO DEL /Q "%%f" >NUL
SET dsl_cl=cl /nologo /c /std:c17 /WX /O2 /MP4 /DLUA_NUMBER=float %*
SET dsl_link=link /nologo /WX
SET dsl_path=_derpy_script_loader
SET dsl_libs=luacore.lib luastd.lib libpng.lib bz2.lib lzma.lib zip.lib zlib.lib zstd.lib d3d9.lib dinput8.lib dwrite.lib advapi32.lib gdi32.lib user32.lib ws2_32.lib
:client
ECHO -- client --
PUSHD src
%dsl_cl% /I..\include *.c library\*.c
IF ERRORLEVEL 1 GOTO failpop
POPD
PUSHD src\client
%dsl_cl% /I..\..\include *.c *.cpp library\*.c
IF ERRORLEVEL 1 GOTO failpop
ECHO derpy_script_loader.asi
%dsl_link% /DLL /OUT:..\..\..\derpy_script_loader.asi /LIBPATH:..\..\lib *.obj ..\*.obj %dsl_libs%
IF ERRORLEVEL 1 GOTO failpop
POPD
:server
ECHO -- server --
PUSHD src
%dsl_cl% /DDSL_SERVER_VERSION /I..\include /MT *.c library\*.c
IF ERRORLEVEL 1 GOTO failpop
POPD
PUSHD src\server
%dsl_cl% /DDSL_SERVER_VERSION /I..\..\include /MT *.c library\*.c
IF ERRORLEVEL 1 GOTO failpop
ECHO derpy_script_server.exe
%dsl_link% /OUT:..\..\server\derpy_script_server.exe /LIBPATH:..\..\lib *.obj ..\*.obj luacore.lib luastd.lib bz2.lib lzma.lib zip.lib zlib.lib zstd.lib advapi32.lib user32.lib ws2_32.lib
IF ERRORLEVEL 1 GOTO failpop
POPD
:success
ECHO success!
FOR /R %%f IN (*.obj) DO DEL /Q "%%f"
GOTO :eof
:failpop
POPD
:failure
ECHO failure!
FOR /R %%f IN (*.obj) DO DEL /Q "%%f"
PAUSE
GOTO :eof
:setupvc
CALL vcvarsall.bat x86 >NUL
IF NOT ERRORLEVEL 1 GOTO aftervc
ECHO You must add the directory containing vcvarsall.bat to your system's path by editing your enviornment variables.
ECHO If you have visual studio installed, it should be in its program files under \VC\Auxiliary\Build.
PAUSE
GOTO :eof
:nobully
ECHO You must run this compilation script in /Bully Scholarship Edition/_derpy_script_loader.
PAUSE