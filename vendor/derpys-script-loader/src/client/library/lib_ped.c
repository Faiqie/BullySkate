// DERPY'S SCRIPT LOADER: PED LIBRARY

#include <dsl/dsl.h>

#define RADIANS_PER_DEGREE (3.14159265358979323846/180.0)

// FUNCTIONS
static int iterPeds(lua_State *lua){
	game_pool *peds;
	int index;
	
	peds = getGamePedPool();
	for(index = lua_tonumber(lua,lua_upvalueindex(1));index < peds->limit;index++){
		lua_pushnumber(lua,index+1);
		lua_replace(lua,lua_upvalueindex(1));
		if(~peds->flags[index] & GAME_POOL_INVALID){
			lua_pushnumber(lua,getGamePedId(peds->array+peds->size*index));
			return 1;
		}
	}
	return 0;
}
static int AllPeds(lua_State *lua){
	lua_pushnumber(lua,0);
	lua_pushcclosure(lua,&iterPeds,1);
	return 1;
}
static int PedCreateScriptless(lua_State *lua){
	void *firstscriptptr;
	script_manager *sm;
	script_block sb;
	dsl_state *dsl;
	void *game;
	int count;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	luaL_checktype(lua,2,LUA_TNUMBER);
	luaL_checktype(lua,3,LUA_TNUMBER);
	luaL_checktype(lua,4,LUA_TNUMBER);
	if(lua_gettop(lua) >= 5)
		luaL_checktype(lua,5,LUA_TNUMBER);
	dsl = getDslState(lua,1);
	sm = dsl->manager;
	game = dsl->game;
	if(!game)
		luaL_error(lua,"game not ready");
	startScriptBlock(sm,NULL,&sb);
	firstscriptptr = *getGameScriptPool(game);
	setGameScriptIndex(game,-1);
	count = getGameScriptCount(game);
	setGameScriptCount(game,0);
	lua_pushnumber(lua,createGameScriptPed(NULL,lua_tonumber(lua,1),lua_tonumber(lua,2),lua_tonumber(lua,3),lua_tonumber(lua,4),lua_tonumber(lua,5)*RADIANS_PER_DEGREE));
	setGameScriptCount(game,count);
	*getGameScriptPool(game) = firstscriptptr;
	finishScriptBlock(sm,&sb,lua);
	return 1;
}
static int PedGetModelId(lua_State *lua){
	char *ped;
	
	ped = getGamePedFromId(lua_tonumber(lua,1),0);
	if(!ped || lua_type(lua,1) != LUA_TNUMBER)
		luaL_typerror(lua,1,"ped");
	lua_pushnumber(lua,*(short*)(ped+0x10E));
	return 1;
}
static int PedGetThrottle(lua_State *lua){
	char *ped;
	char *x;
	
	ped = getGamePedFromId(lua_tonumber(lua,1),0);
	if(!ped || lua_type(lua,1) != LUA_TNUMBER)
		luaL_typerror(lua,1,"ped");
	x = *(char**)(ped+0x2E0);
	lua_pushnumber(lua,*(float*)(x+0x1C));
	return 1;
}
static int PedSetPosSimple(lua_State *lua){
	char *ped;
	
	ped = getGamePedFromId(lua_tonumber(lua,1),0);
	if(!ped || lua_type(lua,1) != LUA_TNUMBER)
		luaL_typerror(lua,1,"ped");
	if(*(int*)(ped+0x1310) == 13){
		*(int*)(ped+0x1310) = 12;
		callGameLuaFunctionPedSetPosXYZ(lua);
		*(int*)(ped+0x1310) = 13;
	}else
		callGameLuaFunctionPedSetPosXYZ(lua);
	return 0;
}
static int PedSetThrottle(lua_State *lua){
	char *ped;
	char *x;
	
	ped = getGamePedFromId(lua_tonumber(lua,1),0);
	if(!ped || lua_type(lua,1) != LUA_TNUMBER)
		luaL_typerror(lua,1,"ped");
	luaL_checktype(lua,2,LUA_TNUMBER);
	x = *(char**)(ped+0x2E0);
	*(float*)(x+0x1C) = lua_tonumber(lua,2);
	return 0;
}
static int PedSpoofModel(lua_State *lua){
	char *ped;
	
	ped = getGamePedFromId(lua_tonumber(lua,1),0);
	if(!ped || lua_type(lua,1) != LUA_TNUMBER)
		luaL_typerror(lua,1,"ped");
	luaL_checktype(lua,2,LUA_TNUMBER);
	return 0;
}

// DEBUG
static int GetPedAddress(lua_State *lua){
	char buffer[32];
	char *ped;
	
	ped = getGamePedFromId(lua_tonumber(lua,1),0);
	if(!ped || lua_type(lua,1) != LUA_TNUMBER)
		luaL_typerror(lua,1,"ped");
	sprintf(buffer,"%p",ped);
	lua_pushstring(lua,buffer);
	return 1;
}

// OPEN
int dslopen_ped(lua_State *lua){
	lua_register(lua,"AllPeds",&AllPeds);
	lua_register(lua,"PedCreateScriptless",&PedCreateScriptless);
	lua_register(lua,"PedGetModelId",&PedGetModelId);
	lua_register(lua,"PedGetThrottle",&PedGetThrottle);
	lua_register(lua,"PedSetPosSimple",&PedSetPosSimple);
	lua_register(lua,"PedSetThrottle",&PedSetThrottle);
	lua_register(lua,"PedSpoofModel",&PedSpoofModel);
	if(getDslState(lua,1)->flags & DSL_ADD_DEBUG_FUNCS)
		lua_register(lua,"GetPedAddress",&GetPedAddress);
	return 0;
}