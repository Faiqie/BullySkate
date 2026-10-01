// DERPY'S SCRIPT LOADER: SAVEDATA LIBRARY

#include <dsl/dsl.h>

static int IsSaveDataReady(lua_State *lua){
	lua_pushboolean(lua,getDslState(lua,1)->save_data != LUA_NOREF);
	return 1;
}
static int GetSaveDataTable(lua_State *lua){
	dsl_state *dsl;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	dsl = getDslState(lua,1);
	lua_rawgeti(lua,LUA_REGISTRYINDEX,dsl->save_data);
	if(!lua_istable(lua,-1))
		luaL_error(lua,"save data is not ready");
	lua_pushvalue(lua,1);
	lua_rawget(lua,-2);
	if(lua_istable(lua,-1))
		return 1;
	lua_pop(lua,1);
	lua_newtable(lua);
	lua_pushvalue(lua,1);
	lua_pushvalue(lua,-2);
	lua_rawset(lua,-4);
	return 1;
}
static int GetRawSaveDataTable(lua_State *lua){
	lua_rawgeti(lua,LUA_REGISTRYINDEX,getDslState(lua,1)->save_data);
	if(!lua_istable(lua,-1))
		luaL_error(lua,"save data is not ready");
	return 1;
}
int dslopen_savedata(lua_State *lua){
	lua_register(lua,"IsSaveDataReady",&IsSaveDataReady); // boolean ()
	lua_register(lua,"GetSaveDataTable",&GetSaveDataTable); // table (string id)
	lua_register(lua,"GetRawSaveDataTable",&GetRawSaveDataTable); // table ()
	return 0;
}