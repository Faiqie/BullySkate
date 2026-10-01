// DERPY'S SCRIPT LOADER: CAMERA LIBRARY

#include <dsl/dsl.h>

// FUNCTIONS
static int CameraGetXYZ(lua_State *lua){
	return 0;
}

// DEBUG
static int GetCameraAddress(lua_State *lua){
	char buffer[32];
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	sprintf(buffer,"%p",getGameCameraObject((int)lua_tonumber(lua,1)));
	lua_pushstring(lua,buffer);
	return 1;
}

// OPEN
int dslopen_camera(lua_State *lua){
	//lua_register(lua,"CameraGetXYZ",&CameraGetXYZ);
	if(getDslState(lua,1)->flags & DSL_ADD_DEBUG_FUNCS)
		lua_register(lua,"GetCameraAddress",&GetCameraAddress);
	return 0;
}