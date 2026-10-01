// DERPY'S SCRIPT LOADER: VEHICLE LIBRARY

#include <dsl/dsl.h>

// FUNCTIONS
static int iterVehicles(lua_State *lua){
	game_pool *vehs;
	int index;
	
	vehs = getGameVehiclePool();
	for(index = lua_tonumber(lua,lua_upvalueindex(1));index < vehs->limit;index++){
		lua_pushnumber(lua,index+1);
		lua_replace(lua,lua_upvalueindex(1));
		if(~vehs->flags[index] & GAME_POOL_INVALID){
			lua_pushnumber(lua,vehs->flags[index] & 0x7F);
			return 1;
		}
	}
	return 0;
}
static int AllVehicles(lua_State *lua){
	lua_pushnumber(lua,0);
	lua_pushcclosure(lua,&iterVehicles,1);
	return 1;
}

// OPEN
int dslopen_vehicle(lua_State *lua){
	//lua_register(lua,"AllVehicles",&AllVehicles);
	return 0;
}