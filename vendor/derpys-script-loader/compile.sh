gcc_options="-m32 -O2 -std=gnu17"
gcc_defines="-D_FILE_OFFSET_BITS=64 -DDSL_SERVER_VERSION -DDSL_DISABLE_ZIP_FILES $@"
gcc_warnings="-Wno-incompatible-pointer-types -Wno-unused-result"

echo compiling shared core...
pushd src > /dev/null
gcc -c $gcc_options $gcc_defines $gcc_warnings -I ../include *.c
echo compiling shared library...
gcc -c $gcc_options $gcc_defines $gcc_warnings -I ../include library/*.c
popd > /dev/null

echo compiling server core...
pushd src/server > /dev/null
gcc -c $gcc_options $gcc_defines $gcc_warnings -I ../../include *.c
echo compiling server library...
gcc -c $gcc_options $gcc_defines $gcc_warnings -I ../../include library/*.c
popd > /dev/null

echo linking derpy_script_server...
gcc $gcc_options -s -o server/derpy_script_server -L lib/a src/server/*.o src/*.o -l luacore -l luastd -l m

echo cleaning up...
rm src/server/*.o src/*.o
echo done