// compile to package.dll and put it in "packages" so it can be loaded by "example.lua"
// see dslpackage.h for information on making DSL packages

#include <dslpackage.h>

static int SetGamePaused(lua_State *lua){
	*(char*)0xC1A99A = lua_toboolean(lua,1);
	return 0;
}
__declspec(dllexport) int luaopen_package(lua_State *lua){
	initDslDll();
	lua_register(lua,"SetGamePaused",&SetGamePaused);
	return 0;
}