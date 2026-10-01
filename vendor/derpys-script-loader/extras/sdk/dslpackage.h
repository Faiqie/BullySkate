/* DERPY'S SCRIPT LOADER: PACKAGE HEADER
	
	about packages:
		if you know how to use Lua's C api to make your own functions you can make your own DLLs to load with DSL
		in this case you should make two files in the "packages" folder, "package.lua" and "package.dll" where "package" is whatever name you want
	
	making package.dll:
		use the included Lua 5.0.2 headers and libraries (the header is stock, and the libraries were compiled with settings to match the game)
		include dslpackage.h and do not include Lua yourself as dslpackage.h already includes it and correctly sets LUA_API and LUA_NUMBER
		call initDslDll() before calling *any* Lua functions or your code could crash the game
		make a lua_CFunction for opening the package using "loadlib" in the package script
	
	making package.lua:
		first call "RequireSystemAccess" before anything to make sure system access was enabled by the user
		then call "loadlib" and "GetPackageFilePath" to load the open function from your DLL
		the returned function can now be called to actually run your DLL
	
	using packages:
		a script can call "require" with "package" to load "package.lua" from the "packages" folder
		this is how scripts should require the use of your custom package
	
*/

#ifndef DSL_PACKAGE_H
#define DSL_PACKAGE_H

// Lua config
#ifdef __cplusplus
#ifndef LUA_API
#define LUA_API extern "C"
#endif
#endif
#ifndef LUA_NUMBER
#define LUA_NUMBER float
#endif

// Lua headers
#include <lua.h>
#include <lauxlib.h>

// Other headers
#include <stdint.h>
#include <windows.h>

// Lua extra declarations
void luaC_collectgarbage();

// Lua patches for Bully SE
static int initDslDllFunction(void *ptr,void *target){
	DWORD reset;
	
	if(VirtualProtect(ptr,5,PAGE_READWRITE,&reset)){
		*(unsigned char*)ptr = 0xE9;
		*(int32_t*)((char*)ptr+1) = (int32_t)target - ((int32_t)ptr+5);
		return VirtualProtect(ptr,5,reset,&reset);
	}
	return 0;
}
static void initDslDll(){
	initDslDllFunction(&lua_close,(void*)0x7420B0);
	initDslDllFunction(&lua_newthread,(void*)0x73AE60);
	initDslDllFunction(&lua_open,(void*)0x742010);
	initDslDllFunction(&luaC_collectgarbage,(void*)0x740F20);
}

#endif