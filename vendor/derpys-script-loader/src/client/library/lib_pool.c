// DERPY'S SCRIPT LOADER: POOL LIBRARY

#include <dsl/dsl.h>

// NAMES
static const struct{
	const char *name;
	game_pool **ptr;
}pools[] = {
	{"ptr_node",(game_pool**)0xC0F5E8}, // 15000 GetPtrNodePool
	{"entry_info_node",(game_pool**)0xC0F5EC}, // 2000  GetEntryInfoNodePool
	{"vehicle",(game_pool**)0xC0F5F4}, // 15    GetVehiclePool
	{"ped",(game_pool**)0xC0F5F0}, // 24    GetPedPool, 8 others
	{"entity_effect_keeper",(game_pool**)0xC0F628},
	{"accessory_container",(game_pool**)0xC0F62C},
	{"attitude_set",(game_pool**)0xC0F630},
	{"ped_action_tree",(game_pool**)0xC0F634},
	{"targeting_system",(game_pool**)0xC0F638},
	{"weapon_accessory_container",(game_pool**)0xC0F63C},
	{"motion_controller",(game_pool**)0xC0F640},
	{"unknown_a",(game_pool**)0xC0F648},
	{"joint_constraint",(game_pool**)0xC0F644}, // 48    GetJointConstraintPool, 3 others
	{"unknown_b",(game_pool**)0xC0F64C},
	{"accessory_pool",(game_pool**)0xC0F60C},
	{"sounds",(game_pool**)0xC0F620},
	{"weapons",(game_pool**)0xC0F654}, // 57    weapons
	{"lua_script",(game_pool**)0xC0F650}, // 8     GetLuaScriptPool
	{"dummy",(game_pool**)0xC0F600}, // 300   GetDummyPool
	{"prop_anim",(game_pool**)0xC0F608}, // 220   GetPropAnimPool
	{"building",(game_pool**)0xC0F5F8}, // 2250  GetBuildingPool
	{"treadable",(game_pool**)0xC0F5FC}, // 1     GetTreadablePool
	{"col_model",(game_pool**)0xC0F604}, // 4150  GetColModelPool
	{"stimulus",(game_pool**)0xC0F610}, // 87    GetStimulusPool
	{"object",(game_pool**)0xC0F614}, // 275   GetObjectPool
	{"projectile",(game_pool**)0xC0F618}, // 35    GetProjectilePool
	{"cut_scene",(game_pool**)0xC0F61C}, // 30    GetCutScenePool
	{"unknown_c",(game_pool**)0xC0F624}, // 200   ?
	{NULL,NULL}
};

// UTILITY
static game_pool* getPoolByName(const char *name){
	if(dslisenum(name,"PTR_NODE"))
		return *(game_pool**)0xC0F5E8; // 15000 GetPtrNodePool
	if(dslisenum(name,"ENTRY_INFO_NODE"))
		return *(game_pool**)0xC0F5EC; // 2000  GetEntryInfoNodePool
	if(dslisenum(name,"VEHICLE"))
		return *(game_pool**)0xC0F5F4; // 15    GetVehiclePool
	if(dslisenum(name,"PED"))
		return *(game_pool**)0xC0F5F0; // 24    GetPedPool, 8 others
	if(dslisenum(name,"JOINT_CONSTRAINT"))
		return *(game_pool**)0xC0F644; // 48    GetJointConstraintPool, 3 others
	if(dslisenum(name,"WEAPON"))
		return *(game_pool**)0xC0F654; // 57    weapons
	if(dslisenum(name,"LUA_SCRIPT"))
		return *(game_pool**)0xC0F650; // 8     GetLuaScriptPool
	if(dslisenum(name,"DUMMY"))
		return *(game_pool**)0xC0F600; // 300   GetDummyPool
	if(dslisenum(name,"PROP_ANIM"))
		return *(game_pool**)0xC0F608; // 220   GetPropAnimPool
	if(dslisenum(name,"BUILDING"))
		return *(game_pool**)0xC0F5F8; // 2250  GetBuildingPool
	if(dslisenum(name,"TREADABLE"))
		return *(game_pool**)0xC0F5FC; // 1     GetTreadablePool
	if(dslisenum(name,"COL_MODEL"))
		return *(game_pool**)0xC0F604; // 4150  GetColModelPool
	if(dslisenum(name,"STIMULUS"))
		return *(game_pool**)0xC0F610; // 87    GetStimulusPool
	if(dslisenum(name,"OBJECT"))
		return *(game_pool**)0xC0F614; // 275   GetObjectPool
	if(dslisenum(name,"PROJECTILE"))
		return *(game_pool**)0xC0F618; // 35    GetProjectilePool
	if(dslisenum(name,"CUT_SCENE"))
		return *(game_pool**)0xC0F61C; // 30    GetCutScenePool
	if(dslisenum(name,"UNKNOWN"))
		return *(game_pool**)0xC0F624; // 200   ?
	return NULL;
}

// FUNCTIONS
static int GetPoolSize(lua_State *lua){
	game_pool *pool;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	pool = getPoolByName(lua_tostring(lua,1));
	if(!pool)
		luaL_argerror(lua,1,"unknown pool");
	lua_pushnumber(lua,pool->limit);
	return 1;
}
static int GetPoolUsage(lua_State *lua){
	game_pool *pool;
	int index;
	int count;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	pool = getPoolByName(lua_tostring(lua,1));
	if(!pool)
		luaL_argerror(lua,1,"unknown pool");
	index = pool->limit;
	count = 0;
	while(index)
		if(pool->flags[--index] & GAME_POOL_INVALID)
			count++;
	lua_pushnumber(lua,pool->limit-count);
	return 1;
}
static int GetPoolSpace(lua_State *lua){
	game_pool *pool;
	int index;
	int count;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	pool = getPoolByName(lua_tostring(lua,1));
	if(!pool)
		luaL_argerror(lua,1,"unknown pool");
	index = pool->limit;
	count = 0;
	while(index)
		if(pool->flags[--index] & GAME_POOL_INVALID)
			count++;
	lua_pushnumber(lua,count);
	return 1;
}
static int GetAllPoolInfo(lua_State *lua){
	int i;
	int index;
	int count;
	game_pool *pool;
	
	lua_newtable(lua);
	index = 0;
	while(pools[index].name){
		pool = *pools[index].ptr;
		lua_newtable(lua);
		lua_pushstring(lua,"name");
		lua_pushstring(lua,pools[index].name);
		lua_rawset(lua,-3);
		lua_pushstring(lua,"size");
		lua_pushnumber(lua,pool->size);
		lua_rawset(lua,-3);
		lua_pushstring(lua,"limit");
		lua_pushnumber(lua,pool->limit);
		lua_rawset(lua,-3);
		for(count = i = 0;i < pool->limit;i++)
			if(pool->flags[i] & GAME_POOL_INVALID)
				count++;
		lua_pushstring(lua,"count");
		lua_pushnumber(lua,pool->limit-count);
		lua_rawset(lua,-3);
		lua_rawseti(lua,-2,++index);
	}
	return 1;
}
static int RequirePoolSize(lua_State *lua){
	game_pool *pool;
	dsl_state *dsl;
	script_manager *sm;
	int desired;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TNUMBER);
	pool = getPoolByName(lua_tostring(lua,1));
	if(!pool)
		luaL_argerror(lua,1,"unknown pool");
	dsl = getDslState(lua,1);
	sm = dsl->manager;
	if(!sm->running_script)
		luaL_error(lua,"no DSL script running");
	desired = lua_tonumber(lua,2);
	if(pool->limit < desired){
		printConsoleError(dsl->console,"%s%s requires a larger %s pool (%d / %d, check DSL config or use your own pool adjuster)",getDslPrintPrefix(dsl,0),getScriptName(sm->running_script),lua_tostring(lua,1),pool->limit,desired);
		lua_pushnil(lua);
		lua_error(lua);
	}
	return 0;
}

// OPEN
int dslopen_pool(lua_State *lua){
	lua_register(lua,"GetPoolSize",&GetPoolSize);
	lua_register(lua,"GetPoolUsage",&GetPoolUsage);
	lua_register(lua,"GetPoolSpace",&GetPoolSpace);
	lua_register(lua,"GetAllPoolInfo",&GetAllPoolInfo);
	lua_register(lua,"RequirePoolSize",&RequirePoolSize);
	return 0;
}