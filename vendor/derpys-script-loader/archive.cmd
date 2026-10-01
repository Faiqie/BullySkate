@ECHO OFF
SET dsl_path=_derpy_script_loader
SET zip_exe="C:\Program Files\7-Zip\7z.exe"
IF EXIST %zip_exe% GOTO archive
ECHO 7-Zip could not be found.
PAUSE
:archive
ECHO derpy_script_loader.zip
IF EXIST derpy_script_loader.zip DEL /Q derpy_script_loader.zip
PUSHD ..
%zip_exe% a -sse -y -mx9 %dsl_path%/derpy_script_loader.zip %dsl_path%/examples %dsl_path%/extras %dsl_path%/scripts %dsl_path%/documentation.html %dsl_path%/signature.png derpy_script_loader.asi dinput8.dll >NUL
POPD
%zip_exe% a -sse -y -mx9 derpy_script_loader.zip install.png readme.txt >NUL
IF NOT EXIST derpy_script_loader.zip GOTO :eof
ECHO derpy_script_server.zip
IF EXIST derpy_script_server.zip DEL /Q derpy_script_server.zip
PUSHD server
%zip_exe% a -sse -y -mx9 ../derpy_script_server.zip scripts derpy_script_server derpy_script_server.exe readme.txt >NUL
POPD
ECHO derpy_script_source.zip
IF EXIST derpy_script_source.zip DEL /Q derpy_script_source.zip
COPY derpy_script_loader.zip derpy_script_source.zip >NUL
PUSHD ..
%zip_exe% a -sse -y -mx9 %dsl_path%/derpy_script_source.zip %dsl_path%/include %dsl_path%/lib %dsl_path%/server %dsl_path%/src %dsl_path%/archive.cmd %dsl_path%/compile.cmd %dsl_path%/compile.sh %dsl_path%/compile_s.cmd %dsl_path%/compile_s.sh %dsl_path%/readme.txt >NUL
POPD
ECHO derpy_script_preview.zip
IF EXIST derpy_script_preview.zip DEL /Q derpy_script_preview.zip
PUSHD ..
%zip_exe% a -sse -y -mx9 %dsl_path%/derpy_script_preview.zip derpy_script_loader.asi dinput8.dll >NUL
POPD
PUSHD server
%zip_exe% a -sse -y -mx9 ../derpy_script_preview.zip derpy_script_server.exe derpy_script_server >NUL
POPD
%zip_exe% a -sse -y -mx9 derpy_script_preview.zip preview.txt >NUL