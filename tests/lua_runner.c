#define LUA_NUMBER float
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
int main(int argc,char **argv){
 lua_State *l=lua_open();int result;if(argc!=2)return 2;
 luaopen_base(l);luaopen_table(l);luaopen_string(l);luaopen_math(l);lua_settop(l,0);
 result=luaL_loadfile(l,argv[1]);if(!result)result=lua_pcall(l,0,0,0);
 if(result)puts(lua_tostring(l,-1));lua_close(l);return result;
}
