// DERPY'S SCRIPT LOADER: EVENT LIBRARY

#include <dsl/dsl.h>

#ifndef DSL_SERVER_VERSION
static int AddScriptLoaderCallback(lua_State *lua){
	dsl_state *dsl;
	script *s;
	
	luaL_checktype(lua,1,LUA_TFUNCTION);
	lua_settop(lua,1);
	dsl = getDslState(lua,1);
	s = dsl->manager->running_script;
	if(!s)
		luaL_error(lua,"cannot add script loader callback here");
	if(!addLuaScriptEventCallback(dsl->events,s,lua,LOCAL_EVENT,"NativeScriptLoaded"))
		luaL_error(lua,"failed to create event handler");
	return 0;
}
#endif
static int registerEventHandler(lua_State *lua,int type){
	dsl_state *dsl;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TFUNCTION);
	lua_settop(lua,2);
	dsl = getDslState(lua,1);
	if(!addLuaScriptEventCallback(dsl->events,dsl->manager->running_script,lua,type,lua_tostring(lua,1)))
		luaL_error(lua,"failed to create event handler");
	return 1;
}
static int RegisterLocalEventHandler(lua_State *lua){
	return registerEventHandler(lua,LOCAL_EVENT);
}
static int RegisterNetworkEventHandler(lua_State *lua){
	return registerEventHandler(lua,REMOTE_EVENT);
}
static int RemoveEventHandler(lua_State *lua){
	if(!lua_isuserdata(lua,1))
		luaL_typerror(lua,1,"event handler");
	lua_settop(lua,1);
	removeLuaScriptEventCallback(getDslState(lua,1)->events,lua);
	return 0;
}
static int RunLocalEvent(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	lua_pushboolean(lua,runLuaScriptEvent(getDslState(lua,1)->events,lua,LOCAL_EVENT,lua_tostring(lua,1),lua_gettop(lua)-1));
	return 1;
}
static int SendNetworkEvent(lua_State *lua){
	return 0;
}
static int GetScriptEventDebug(lua_State *lua){
	debugLuaScriptEvents(getDslState(lua,1)->events,lua);
	return 1;
}
int dslopen_event(lua_State *lua){
	#ifndef DSL_SERVER_VERSION
	lua_register(lua,"AddScriptLoaderCallback",&AddScriptLoaderCallback); // void (function callback)
	#endif
	lua_register(lua,"RegisterLocalEventHandler",&RegisterLocalEventHandler); // handler (string event, function callback)
	lua_register(lua,"RegisterNetworkEventHandler",&RegisterNetworkEventHandler); // handler (string event, function callback)
	lua_register(lua,"RemoveEventHandler",&RemoveEventHandler); // void (handler handler)
	lua_register(lua,"RunLocalEvent",&RunLocalEvent); // boolean (string event, any args...)
	if(getDslState(lua,1)->flags & DSL_ADD_DEBUG_FUNCS)
		lua_register(lua,"GetScriptEventDebug",&GetScriptEventDebug); // table ()
	return 0;
}