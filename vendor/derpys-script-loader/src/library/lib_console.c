// DERPY'S SCRIPT LOADER: CONSOLE LIBRARY

#include <dsl/dsl.h>

#ifndef DSL_SERVER_VERSION
static int IsConsoleActive(lua_State *lua){
	lua_pushboolean(lua,getDslState(lua,1)->show_console);
	return 1;
}
#endif
static int print(lua_State *lua,int type){
	dsl_state *dsl;
	int i,n;
	
	if(!lua_checkstack(lua,lua_gettop(lua)*2+LUA_MINSTACK)) // because every arg can push 2 strings
		luaL_error(lua,"too many arguments");
	lua_pushstring(lua,"tostring");
	lua_gettable(lua,LUA_GLOBALSINDEX);
	if(lua_isfunction(lua,-1)){
		lua_insert(lua,1);
		for(i = 2,n = lua_gettop(lua);i <= n;i++){
			lua_pushvalue(lua,1);
			lua_pushvalue(lua,i);
			lua_call(lua,1,1);
			if(!lua_isstring(lua,-1))
				lua_pop(lua,i > 2 ? 2 : 1);
			else if(i < n)
				lua_pushstring(lua,"\t");
		}
		lua_concat(lua,lua_gettop(lua)-n);
		lua_replace(lua,1);
	}else{
		lua_pop(lua,1);
		luaL_checktype(lua,1,LUA_TSTRING);
	}
	if(*lua_tostring(lua,1)){
		dsl = getDslState(lua,1);
		lua_settop(lua,1);
		if(type == CONSOLE_OUTPUT)
			lua_pushstring(lua,"output");
		else if(type == CONSOLE_ERROR)
			lua_pushstring(lua,"error");
		else if(type == CONSOLE_WARNING)
			lua_pushstring(lua,"warning");
		else
			lua_pushstring(lua,"special");
		runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"ScriptPrinted",2); // message, type
		lua_pop(lua,1); // pop type
		lua_pushstring(lua,getDslPrintPrefix(dsl,type == CONSOLE_ERROR));
		lua_insert(lua,1);
		lua_concat(lua,2);
		printConsoleRaw(dsl->console,type,lua_tostring(lua,1));
	}
	return 0;
}
static int PrintOutput(lua_State *lua){
	return print(lua,CONSOLE_OUTPUT);
}
static int PrintWarning(lua_State *lua){
	return print(lua,CONSOLE_WARNING);
}
static int PrintError(lua_State *lua){
	return print(lua,CONSOLE_ERROR);
}
static int PrintSpecial(lua_State *lua){
	return print(lua,CONSOLE_SPECIAL);
}
int dslopen_console(lua_State *lua){
	#ifndef DSL_SERVER_VERSION
	lua_register(lua,"IsConsoleActive",&IsConsoleActive); // boolean ()
	#endif
	lua_register(lua,"PrintOutput",&PrintOutput); // void (any text...)
	lua_register(lua,"print",&PrintOutput); // void (any text...)
	lua_register(lua,"PrintWarning",&PrintWarning); // void (any text...)
	lua_register(lua,"PrintError",&PrintError); // void (any text...)
	lua_register(lua,"PrintSpecial",&PrintSpecial); // void (any text...)
	return 0;
}