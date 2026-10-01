// DERPY'S SCRIPT LOADER: SERIALIZE LIBRARY

#include <dsl/dsl.h>

static int PackData(lua_State *lua){
	size_t bytes;
	void *data;
	int i,n;
	
	n = lua_gettop(lua);
	lua_newtable(lua);
	for(i = 1;i <= n;i++){
		lua_pushvalue(lua,i);
		lua_rawseti(lua,-2,i);
	}
	lua_pushstring(lua,"n");
	lua_pushnumber(lua,n);
	lua_rawset(lua,-3);
	data = packLuaTable(lua,&bytes);
	if(!data)
		luaL_error(lua,"%s",lua_tostring(lua,-1));
	lua_pushlstring(lua,data,bytes);
	free(data);
	return 1;
}
static int UnpackData(lua_State *lua){
	int i,n;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	lua_settop(lua,1);
	if(!unpackLuaTable(lua,lua_tostring(lua,1),lua_strlen(lua,1)))
		luaL_error(lua,"%s",lua_tostring(lua,-1));
	lua_pushstring(lua,"n");
	lua_rawget(lua,2);
	if(!lua_isnumber(lua,3))
		luaL_argerror(lua,1,"data table missing \"n\"");
	n = lua_tonumber(lua,3);
	if(n < 0 || !lua_checkstack(lua,n+LUA_MINSTACK))
		luaL_error(lua,"failed to unpack data table");
	for(i = 1;i <= n;i++)
		lua_rawgeti(lua,2,i);
	return n;
}
static int PackTable(lua_State *lua){
	size_t bytes;
	void *data;
	
	luaL_checktype(lua,1,LUA_TTABLE);
	lua_settop(lua,1);
	data = packLuaTable(lua,&bytes);
	if(!data)
		luaL_error(lua,"%s",lua_tostring(lua,-1));
	lua_pushlstring(lua,data,bytes);
	free(data);
	return 1;
}
static int UnpackTable(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	lua_settop(lua,1);
	if(!unpackLuaTable(lua,lua_tostring(lua,1),lua_strlen(lua,1)))
		luaL_error(lua,"%s",lua_tostring(lua,-1));
	return 1;
}
int dslopen_serialize(lua_State *lua){
	lua_register(lua,"PackData",&PackData); // string (any args...)
	lua_register(lua,"UnpackData",&UnpackData); // any... (string data)
	lua_register(lua,"PackTable",&PackTable); // string (table table)
	lua_register(lua,"UnpackTable",&UnpackTable); // table (string data)
	return 0;
}