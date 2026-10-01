// DERPY'S SCRIPT LOADER: MISCELLANEOUS LIBRARY

#include <dsl/dsl.h>
#include <string.h>
#ifndef _WIN32
#include <ctype.h>
#include <time.h>
#endif

// MISCELLANEOUS
static int IsHash(lua_State *lua){
	if(lua_type(lua,1) != LUA_TLIGHTUSERDATA)
		luaL_typerror(lua,1,"hash");
	luaL_checktype(lua,2,LUA_TSTRING);
	lua_pushboolean(lua,lua_touserdata(lua,1) == (void*)strtoul(lua_tostring(lua,2),NULL,16));
	return 1;
}
static int GetHash(lua_State *lua){
	const char *str;
	char c;
	int x;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	str = lua_tostring(lua,1);
	for(x = 0;c = *str;str++){
		x *= 0x83;
		x += c;
	}
	x &= 0x7FFFFFFF;
	lua_pushlightuserdata(lua,(void*)x);
	return 1;
}
static int GetFrameTime(lua_State *lua){
	lua_pushnumber(lua,getDslState(lua,1)->frame_time/1000.0f);
	return 1;
}
static int GetPersistentDataTable(lua_State *lua){
	dsl_state *dsl;
	script_collection *sc;
	
	dsl = getDslState(lua,1);
	sc = dsl->manager->running_collection;
	if(!sc)
		luaL_error(lua,"no DSL script running");
	lua_rawgeti(lua,LUA_REGISTRYINDEX,dsl->dsl_data);
	if(!lua_istable(lua,-1)){
		lua_pop(lua,1);
		lua_newtable(lua);
		lua_pushvalue(lua,-1);
		dsl->dsl_data = luaL_ref(lua,LUA_REGISTRYINDEX);
	}
	lua_pushstring(lua,getScriptCollectionName(sc));
	lua_rawget(lua,-2);
	if(lua_istable(lua,-1))
		return 1;
	lua_pop(lua,1);
	lua_newtable(lua);
	lua_pushstring(lua,getScriptCollectionName(sc));
	lua_pushvalue(lua,-2);
	lua_rawset(lua,-4);
	return 1;
}
static int GetSystemTimer(lua_State *lua){
	#ifdef _WIN32
	lua_pushnumber(lua,GetTickCount());
	#else
	lua_pushnumber(lua,clock());
	#endif
	return 1;
}
static int IsDslScriptRunning(lua_State *lua){
	lua_pushboolean(lua,getDslState(lua,1)->manager->running_script != NULL);
	return 1;
}
#ifdef DSL_SERVER_VERSION
static int GetTimer(lua_State *lua){
	lua_Number timer;
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	timer = dsl->last_frame;
	if(dsl->t_wrapped)
		timer += dsl->t_wrapped + dsl->t_wrapped * (lua_Number)((DWORD)-1);
	lua_pushnumber(lua,timer);
	return 1;
}
static int ObjectNameToHashID(lua_State *lua){
	const char *str;
	char c;
	int x;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	str = lua_tostring(lua,1);
	for(x = 0;c = *str;str++){
		x *= 0x83;
		x += toupper(c);
	}
	x &= 0x7FFFFFFF;
	lua_pushlightuserdata(lua,(void*)x);
	return 1;
}
static int QuitServer(lua_State *lua){
	getDslState(lua,1)->flags &= ~DSL_RUN_MAIN_LOOP;
	return 0;
}
#else
static int ForceWindowUpdate(lua_State *lua){
	runDslWindowStyleEvent(getDslState(lua,1),getGameWindow(),1);
	return 0;
}
static int GetInternalResolution(lua_State *lua){
	lua_pushnumber(lua,getGameWidth());
	lua_pushnumber(lua,getGameHeight());
	return 2;
}
static int GetPlayerName(lua_State *lua){
	const char *name;
	void *ptr;
	
	ptr = getDslState(lua,1)->config;
	if(name = getConfigString(ptr,"username"))
		lua_pushstring(lua,name);
	else
		lua_pushstring(lua,"player");
	return 1;
}
static int GetScreenResolution(lua_State *lua){
	lua_pushnumber(lua,GetSystemMetrics(SM_CXSCREEN));
	lua_pushnumber(lua,GetSystemMetrics(SM_CYSCREEN));
	return 2;
}
static int IsActionNodeValid(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	lua_pushboolean(lua,isGameActionNodeValid(lua_tostring(lua,1)));
	return 1;
}
static int IsGamePaused(lua_State *lua){
	lua_pushboolean(lua,getGamePaused());
	return 1;
}
static int QuitGame(lua_State *lua){
	exit(lua_tonumber(lua,1));
	return 1;
}
#endif

// COMPATIBILITY
#ifndef DSL_SERVER_VERSION
static int callFunctionFromDslScript(lua_State *lua,dsl_state *dsl,script *s){
	script_block sb;
	lua_State *co;
	char *object;
	int results;
	int stack;
	int stackb4;
	
	// TODO: check this for errors because i guess we got failed to pass arguments during play testing?
	
	if(!s || s->flags & SHUTDOWN_SCRIPT)
		luaL_argerror(lua,1,"invalid script");
	object = s->script_object;
	stack = lua_gettop(lua);
	if(*(int*)(object+0x1148)){
		*(int*)(object+0x114C) = 0;
		co = *(lua_State**)(object+0x48);
		if(!lua_checkstack(co,stack+LUA_MINSTACK))
			luaL_error(lua,"failed to pass arguments");
		stackb4 = lua_gettop(co);
		lua_xmove(lua,co,stack);
	}else
		co = lua;
	startScriptBlock(dsl->manager,s,&sb);
	if(lua_pcall(co,stack-2,LUA_MULTRET,0)){
		results = -1;
		if(co != lua)
			lua_xmove(co,lua,1); // error object
	}else{
		results = lua_gettop(co) - 1; // don't count the script name arg as a result
		if(co != lua){
			results -= stackb4; // don't count whatever was on the thread before as a result
			if(lua_checkstack(lua,results+LUA_MINSTACK))
				lua_xmove(co,lua,results);
			else
				results = -2;
		}
	}
	finishScriptBlock(dsl->manager,&sb,lua);
	if(results == -1)
		lua_error(lua);
	if(results == -2)
		luaL_error(lua,"failed to pass results");
	return results;
}
static int CallFunctionFromScript(lua_State *lua){
	lua_State *co;
	dsl_state *dsl;
	script_block sb;
	const char *name;
	void *game;
	void **scripts;
	void *firstscriptptr;
	int count;
	int index;
	int stack;
	int stackb4;
	int results;
	script *s;
	
	if(!lua_isnil(lua,1) && !lua_isuserdata(lua,1) && lua_type(lua,1) != LUA_TSTRING)
		luaL_typerror(lua,1,lua_pushfstring(lua,"%s or script",lua_typename(lua,LUA_TSTRING)));
	luaL_checktype(lua,2,LUA_TFUNCTION);
	dsl = getDslState(lua,1);
	if(lua_isuserdata(lua,1))
		return callFunctionFromDslScript(lua,dsl,*(script**)lua_touserdata(lua,1));
	name = lua_tostring(lua,1);
	game = dsl->game;
	if(!game)
		luaL_error(lua,"game not ready");
	scripts = getGameScriptPool(game);
	count = getGameScriptCount(game);
	stack = lua_gettop(lua);
	if(name){
		for(index = 0;index < count;index++)
			if(!strcmp((char*)scripts[index]+4,name))
				break;
		if(index == count)
			luaL_argerror(lua,1,lua_pushfstring(lua,"no native script named \"%s\" is running",name));
		if(*(int*)((char*)scripts[index]+0x1148)){
			*(int*)((char*)scripts[index]+0x114C) = 0;
			co = *(lua_State**)((char*)scripts[index]+0x48);
		}else
			co = lua;
		if(co != lua){
			if(!lua_checkstack(co,stack+LUA_MINSTACK))
				luaL_error(lua,"failed to pass arguments");
			stackb4 = lua_gettop(co);
			lua_xmove(lua,co,stack);
		}
	}else
		co = lua;
	startScriptBlock(dsl->manager,NULL,&sb);
	if(name)
		setGameScriptIndex(game,index);
	else{
		firstscriptptr = *scripts; // because a dsl_script could be put here from running a script block, creating native scripts is undefined
		setGameScriptIndex(game,-1);
		count = getGameScriptCount(game);
		setGameScriptCount(game,0);
	}
	if(lua_pcall(co,stack-2,LUA_MULTRET,0)){
		results = -1;
		if(co != lua)
			lua_xmove(co,lua,1); // error object
	}else{
		results = lua_gettop(co) - 1; // don't count the script name arg as a result
		if(co != lua){
			results -= stackb4; // don't count whatever was on the thread before as a result
			if(lua_checkstack(lua,results+LUA_MINSTACK))
				lua_xmove(co,lua,results);
			else
				results = -2;
		}
	}
	if(!name){
		setGameScriptCount(game,count);
		*scripts = firstscriptptr;
	}
	finishScriptBlock(dsl->manager,&sb,lua);
	if(results == -1)
		lua_error(lua);
	if(results == -2)
		luaL_error(lua,"failed to pass results");
	return results;
}
static int proxyFunction(lua_State *lua){
	lua_pushvalue(lua,lua_upvalueindex(1));
	lua_insert(lua,1);
	lua_pushvalue(lua,lua_upvalueindex(2));
	lua_gettable(lua,LUA_GLOBALSINDEX);
	if(!lua_isfunction(lua,-1))
		luaL_error(lua,"`%s' does not exist",lua_tostring(lua,lua_upvalueindex(2)));
	lua_insert(lua,2);
	return CallFunctionFromScript(lua);
}
static int UseProxyScriptForFunction(lua_State *lua){
	lua_Debug ar;
	
	if(!lua_isnil(lua,1))
		luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TSTRING);
	lua_settop(lua,2);
	lua_pushvalue(lua,1);
	lua_pushvalue(lua,2);
	lua_pushcclosure(lua,&proxyFunction,2); // script, name
	if(!lua_getstack(lua,1,&ar) || !lua_getinfo(lua,"f",&ar))
		luaL_error(lua,"failed to get calling environment");
	lua_getfenv(lua,-1);
	lua_replace(lua,-2); // script, name, closure, environment
	lua_insert(lua,-3); // script, environment, name, closure
	lua_rawset(lua,-3); // environment[name] = closure
	return 0;
}
#endif

// DEBUG
#ifndef DSL_SERVER_VERSION
struct lua_functions{
	const char *name;
	lua_CFunction func;
};
static int GetNativeScripts(lua_State *lua){
	void *game;
	void **pool;
	int count;
	int index;
	
	game = getDslState(lua,1)->game;
	if(!game)
		luaL_error(lua,"game not ready");
	lua_newtable(lua);
	pool = getGameScriptPool(game);
	count = getGameScriptCount(game);
	index = 0;
	while(index < count){
		lua_pushstring(lua,(char*)pool[index]+4);
		lua_rawseti(lua,-2,++index);
	}
	lua_pushnumber(lua,getGameScriptIndex(game));
	return 2;
}
static int GetLuaFunctions(lua_State *lua){
	struct lua_functions *address;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	address = (struct lua_functions*)strtoul(lua_tostring(lua,1),NULL,0);
	if(!address)
		luaL_argerror(lua,1,"bad address");
	lua_newtable(lua);
	while(address->name){
		lua_pushstring(lua,address->name);
		lua_pushcfunction(lua,address->func);
		lua_rawset(lua,-3);
		address++;
	}
	return 1;
}
#endif
static int GetRegistry(lua_State *lua){
	lua_pushvalue(lua,LUA_REGISTRYINDEX);
	return 1;
}

// OPEN
int dslopen_miscellaneous(lua_State *lua){
	// miscellaneous
	lua_register(lua,"IsHash",&IsHash);
	lua_register(lua,"GetHash",&GetHash);
	lua_register(lua,"GetFrameTime",&GetFrameTime); // number ()
	lua_register(lua,"GetPersistentDataTable",&GetPersistentDataTable); // table ()
	lua_register(lua,"GetSystemTimer",&GetSystemTimer); // number ()
	lua_register(lua,"IsDslScriptRunning",&IsDslScriptRunning); // boolean ()
	#ifdef DSL_SERVER_VERSION
	lua_register(lua,"GetTimer",&GetTimer);
	lua_register(lua,"ObjectNameToHashID",&ObjectNameToHashID);
	lua_register(lua,"QuitServer",&QuitServer);
	#else
	lua_register(lua,"ForceWindowUpdate",&ForceWindowUpdate);
	lua_register(lua,"GetInternalResolution",&GetInternalResolution);
	lua_register(lua,"GetPlayerName",&GetPlayerName); // string ()
	lua_register(lua,"GetScreenResolution",&GetScreenResolution);
	lua_register(lua,"IsActionNodeValid",&IsActionNodeValid); // boolean (node)
	lua_register(lua,"IsGamePaused",&IsGamePaused); // boolean ()
	lua_register(lua,"QuitGame",&QuitGame);
	// compatibility
	lua_register(lua,"CallFunctionFromScript",&CallFunctionFromScript); // ... (name*,func)
	lua_register(lua,"UseProxyScriptForFunction",&UseProxyScriptForFunction); // void (func_name)
	#endif
	// debug
	if(getDslState(lua,1)->flags & DSL_ADD_DEBUG_FUNCS){
		#ifndef DSL_SERVER_VERSION
		lua_register(lua,"GetNativeScripts",&GetNativeScripts);
		lua_register(lua,"GetLuaFunctions",&GetLuaFunctions);
		#endif
		lua_register(lua,"GetRegistry",&GetRegistry);
	}
	return 0;
}
