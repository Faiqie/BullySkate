// DERPY'S SCRIPT LOADER: MANAGER LIBRARY

#include <dsl/dsl.h>
#include <string.h>

// UTILITY
static script_manager* getActiveManager(lua_State *lua,int required){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	if(required == 2 ? dsl->manager->running_thread != NULL : dsl->manager->running_script != NULL)
		return dsl->manager;
	if(required)
		luaL_error(lua,required == 2 ? "no DSL thread running" : "no DSL script running");
	return NULL;
}
static void getScriptEnvironment(lua_State *lua,script *s){
	lua_pushlightuserdata(lua,s);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	if(!lua_istable(lua,-1))
		luaL_error(lua,"lost script environment");
}
static thread* getThreadFromThread(lua_State *lua,int stack){
	thread *t;
	
	lua_pushvalue(lua,stack);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	t = lua_touserdata(lua,-1);
	lua_pop(lua,1);
	return t;
}

// COLLECTIONS - EXPORTS
static int dsl_shared_index(lua_State *lua){
	script_collection *c;
	const char *str;
	
	if(str = lua_tostring(lua,2))
		for(c = getDslState(lua,1)->manager->collections;c;c = c->next)
			if(c->flags & EXPORT_COLLECTION && !dslstrcmp(str,getScriptCollectionName(c))){
				if(c->flags & SHUTDOWN_COLLECTION)
					return 0;
				lua_pushlightuserdata(lua,c);
				lua_rawget(lua,LUA_REGISTRYINDEX);
				if(lua_istable(lua,-1))
					return 1;
				lua_pop(lua,1);
			}
	return 0;
}
static int dsl_shared_newindex(lua_State *lua){
	luaL_error(lua,"attempt to set value in `dsl'");
	return 0;
}
static int net_shared_index(lua_State *lua){
	script_collection *c;
	const char *str;
	
	if(str = lua_tostring(lua,2))
		for(c = getDslState(lua,1)->manager->collections;c;c = c->next)
			if(c->net != LUA_NOREF && !dslstrcmp(str,getScriptCollectionName(c))){
				if(c->flags & SHUTDOWN_COLLECTION)
					return 0;
				lua_rawgeti(lua,LUA_REGISTRYINDEX,c->net);
				if(lua_istable(lua,-1))
					return 1;
				lua_pop(lua,1);
			}
	return 0;
}
static int net_shared_newindex(lua_State *lua){
	luaL_error(lua,"attempt to set value in `net'");
	return 0;
}

// SCRIPTS - HEADERS
static int requireExactLoaderVersion(lua_State *lua,script_manager *sm,int version){
	if(DSL_VERSION != version){
		printConsoleError(getDslState(lua,1)->console,"%s%s requires DSL%d (running DSL%d, script not backwards compatible)",getDslPrintPrefix(getDslState(lua,1),0),getScriptName(sm->running_script),version,DSL_VERSION);
		lua_pushnil(lua);
		lua_error(lua);
	}
	return 0;
}
static int dsl_AllowOnServer(lua_State *lua){
	loader_collection *lc;
	script_manager *sm;
	
	sm = getActiveManager(lua,1);
	if(lc = sm->running_collection->lc)
		lc->flags |= LOADER_ALLOWSERVER;
	return 0;
}
static int dsl_DontAutoStartScript(lua_State *lua){
	script_manager *sm;
	
	sm = getActiveManager(lua,1);
	if(sm->running_script->flags & INITIAL_SCRIPT_RUN){
		lua_pushnil(lua);
		lua_error(lua);
	}
	return 0;
}
static int dsl_RequireDependency(lua_State *lua){
	dsl_state *dsl;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	dsl = getDslState(lua,1);
	#ifndef DSL_SERVER_VERSION
	if(dsl->network && isNetworkPlayingOnServer(dsl->network))
		luaL_error(lua,"a client script cannot require a dependency");
	#endif
	if(!startScriptLoaderDependency(dsl->loader,getActiveManager(lua,1)->running_collection,lua_tostring(lua,1))){
		lua_pushnil(lua);
		lua_error(lua);
	}
	return 0;
}
static int dsl_RequireLoaderVersion(lua_State *lua){
	script_manager *sm;
	int version;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	version = lua_tonumber(lua,1);
	sm = getActiveManager(lua,1);
	if(lua_toboolean(lua,2))
		return requireExactLoaderVersion(lua,sm,version);
	if(DSL_VERSION < version){
		printConsoleError(getDslState(lua,1)->console,"%s%s requires DSL%d (running DSL%d)",getDslPrintPrefix(getDslState(lua,1),0),getScriptName(sm->running_script),version,DSL_VERSION);
		lua_pushnil(lua);
		lua_error(lua);
	}
	return 0;
}
static int dsl_RequireSystemAccess(lua_State *lua){
	script_manager *sm;
	
	sm = getActiveManager(lua,1);
	if(~getDslState(lua,1)->flags & DSL_SYSTEM_ACCESS){
		printConsoleError(getDslState(lua,1)->console,"%s%s requires system access (check DSL config)",getDslPrintPrefix(getDslState(lua,1),0),getScriptName(sm->running_script));
		lua_pushnil(lua);
		lua_error(lua);
	}
	return 0;
}

// SCRIPTS - INVOCATION
static dsl_file* openScriptFile(lua_State *lua,script_manager *sm){
	const char *name;
	dsl_file *file;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	name = lua_tostring(lua,1);
	file = openDslFile(getDslState(lua,1),NULL,name,"rb",NULL);
	if(!file)
		luaL_argerror(lua,1,lua_pushfstring(lua,"%s: %s",getDslFileError(),name));
	return file;
}
static int createScriptObject(lua_State *lua,script *s){
	getScriptUserdata(lua,s);
	return 1;
}
#ifndef DSL_SERVER_VERSION
static int dsl_ImportScript(lua_State *lua){
	script_manager *sm;
	
	sm = getActiveManager(lua,0);
	if(!sm || sm->use_base_funcs)
		return callGameLuaFunctionImportScript(lua);
	luaL_checktype(lua,1,LUA_TSTRING);
	callGameLuaFunctionImportScript(lua);
	if(!lua_isfunction(lua,-1))
		luaL_error(lua,"failed to import script");
	getScriptEnvironment(lua,sm->running_script);
	lua_setfenv(lua,-2);
	lua_call(lua,0,0);
	return 0;
}
#endif
static int dsl_LoadScript(lua_State *lua){
	script_manager *sm;
	dsl_file *f;
	
	sm = getActiveManager(lua,1);
	f = openScriptFile(lua,sm);
	if(importScript(sm,f,lua_tostring(lua,1),lua)){
		closeDslFile(f);
		return 0;
	}
	closeDslFile(f);
	lua_error(lua);
	return 0;
}
static int dsl_StartScript(lua_State *lua){
	script_manager *sm;
	dsl_file *f;
	script *s;
	int dwr;
	
	sm = getActiveManager(lua,1);
	f = openScriptFile(lua,sm);
	if(lua_gettop(lua) >= 2){
		luaL_checktype(lua,2,LUA_TTABLE);
		lua_settop(lua,2);
	}
	s = createScript(sm->running_collection,0,lua_istable(lua,2),f,lua_tostring(lua,1),lua,&dwr);
	closeDslFile(f);
	if(s)
		return createScriptObject(lua,s);
	if(!dwr)
		lua_error(lua);
	return 0;
}
static int dsl_StartVirtualScript(lua_State *lua){
	script_manager *sm;
	script *s;
	int dwr;
	
	sm = getActiveManager(lua,1);
	if(lua_gettop(lua) >= 2){
		luaL_checktype(lua,1,LUA_TSTRING);
		luaL_checktype(lua,2,LUA_TFUNCTION);
		lua_settop(lua,2);
	}else{
		luaL_checktype(lua,lua_type(lua,1) == LUA_TSTRING ? 2 : 1,LUA_TFUNCTION);
		lua_pushstring(lua,"virtual");
		lua_insert(lua,1);
	}
	if(s = createScript(sm->running_collection,0,0,NULL,lua_tostring(lua,1),lua,&dwr))
		return createScriptObject(lua,s);
	if(!dwr)
		lua_error(lua);
	return 0;
}

// SCRIPTS - CLEANUP
static int dsl_StopCurrentScriptCollection(lua_State *lua){
	if(!stopScriptLoaderCollection(getDslState(lua,1)->loader,getActiveManager(lua,1)->running_collection))
		luaL_error(lua,"failed to stop current script collection");
	return 0;
}
static int dsl_TerminateCurrentScript(lua_State *lua){
	script_manager *sm;
	
	#ifdef DSL_SERVER_VERSION
	sm = getActiveManager(lua,1);
	#else
	sm = getActiveManager(lua,0);
	if(!sm || sm->use_base_funcs)
		return callGameLuaFunctionTerminateCurrentScript(lua);
	#endif
	shutdownScript(sm->running_script,lua,1);
	return 0;
}
static int dsl_TerminateScript(lua_State *lua){
	script_manager *sm;
	script *s;
	
	if(lua_gettop(lua)){
		if(!lua_isuserdata(lua,1))
			luaL_typerror(lua,1,"script");
		if(s = *(script**)lua_touserdata(lua,1))
			shutdownScript(s,lua,1);
	}else if(sm = getActiveManager(lua,1))
		shutdownScript(sm->running_script,lua,1);
	return 0;
}

// SCRIPTS - QUERY
static int dsl_GetCurrentScript(lua_State *lua){
	getScriptUserdata(lua,getActiveManager(lua,1)->running_script);
	return 1;
}
static int dsl_GetScriptCollection(lua_State *lua){
	script_manager *sm;
	script *s;
	
	if(lua_gettop(lua)){
		if(!lua_isuserdata(lua,1))
			luaL_typerror(lua,1,"script");
		s = *(script**)lua_touserdata(lua,1);
		if(!s || s->flags & SHUTDOWN_SCRIPT)
			luaL_error(lua,"invalid script");
		lua_pushstring(lua,getScriptCollectionName(s->collection));
	}else if(sm = getActiveManager(lua,1))
		lua_pushstring(lua,getScriptCollectionName(sm->running_collection));
	return 1;
}
static int dsl_GetScriptEnvironment(lua_State *lua){
	script_manager *sm;
	script *s;
	
	if(lua_gettop(lua)){
		if(!lua_isuserdata(lua,1))
			luaL_typerror(lua,1,"script");
		s = *(script**)lua_touserdata(lua,1);
		if(!s || s->flags & SHUTDOWN_SCRIPT)
			luaL_error(lua,"invalid script");
		getScriptEnvironment(lua,s);
	}else if(sm = getActiveManager(lua,1))
		getScriptEnvironment(lua,sm->running_script);
	return 1;
}
static int dsl_GetScriptName(lua_State *lua){
	script_manager *sm;
	script *s;
	
	if(lua_gettop(lua)){
		if(!lua_isuserdata(lua,1))
			luaL_typerror(lua,1,"script");
		s = *(script**)lua_touserdata(lua,1);
		if(!s || s->flags & SHUTDOWN_SCRIPT)
			luaL_error(lua,"invalid script");
		lua_pushstring(lua,getScriptName(s));
	}else if(sm = getActiveManager(lua,1))
		lua_pushstring(lua,getScriptName(sm->running_script));
	return 1;
}
static int dsl_GetScriptSharedTable(lua_State *lua){
	script_collection *c;
	
	if(lua_gettop(lua) && !lua_isnil(lua,1))
		luaL_checktype(lua,1,LUA_TBOOLEAN);
	c = getActiveManager(lua,1)->running_collection;
	lua_pushlightuserdata(lua,c);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	if(!lua_istable(lua,-1)){
		if(!lua_toboolean(lua,1))
			c->flags |= EXPORT_COLLECTION;
		lua_newtable(lua);
		lua_pushlightuserdata(lua,c);
		lua_pushvalue(lua,-2);
		lua_rawset(lua,LUA_REGISTRYINDEX); // registry[c*] = {}
	}else if(lua_isboolean(lua,1) && lua_toboolean(lua,1) != !(c->flags & EXPORT_COLLECTION))
		luaL_error(lua,"attempt to change share mode");
	return 1;
}
static int dsl_GetScriptNetworkTable(lua_State *lua){
	loader_collection *lc;
	script_collection *c;
	
	c = getActiveManager(lua,1)->running_collection;
	if(c->net == LUA_NOREF){
		#ifndef DSL_SERVER_VERSION
		lc = c->lc;
		if(!lc || ~lc->flags & LOADER_NETWORK)
			luaL_error(lua,"not a network script");
		#endif
		lua_newtable(lua);
		lua_pushvalue(lua,-1);
		c->net = luaL_ref(lua,LUA_REGISTRYINDEX);
		return 1;
	}
	lua_rawgeti(lua,LUA_REGISTRYINDEX,c->net);
	if(!lua_istable(lua,-1))
		luaL_error(lua,"corrupted network table");
	return 1;
}
static int dsl_IsScriptRunning(lua_State *lua){
	script *s;
	
	if(!lua_isuserdata(lua,1))
		luaL_typerror(lua,1,"script");
	s = *(script**)lua_touserdata(lua,1);
	lua_pushboolean(lua,s && ~s->flags & SHUTDOWN_SCRIPT);
	return 1;
}
static int dsl_IsScriptZipped(lua_State *lua){
	loader_collection *lc;
	script_manager *sm;
	script *s;
	
	if(lua_gettop(lua)){
		if(!lua_isuserdata(lua,1))
			luaL_typerror(lua,1,"script");
		s = *(script**)lua_touserdata(lua,1);
		if(!s || s->flags & SHUTDOWN_SCRIPT)
			luaL_error(lua,"invalid script");
		lc = s->collection->lc;
	}else if(sm = getActiveManager(lua,1))
		lc = sm->running_collection->lc;
	lua_pushboolean(lua,lc && lc->zip);
	return 1;
}

// THREADS - CREATION
static int createThreadOfType(lua_State *lua,script_manager *sm,int type,int stack){
	int named;
	
	if(named = lua_type(lua,stack) == LUA_TSTRING){
		getScriptEnvironment(lua,sm->running_script);
		lua_pushvalue(lua,stack);
		lua_gettable(lua,-2);
		lua_insert(lua,stack+1); // right after the name
		lua_pop(lua,1);
		if(!lua_isfunction(lua,stack+1))
			luaL_typerror(lua,stack,lua_typename(lua,LUA_TFUNCTION));
	}else
		luaL_checktype(lua,stack,LUA_TFUNCTION);
	if(lua_iscfunction(lua,stack))
		luaL_typerror(lua,1,"lua function");
	if(!createThread(sm->running_script,lua,lua_gettop(lua)-stack-named,type,0,lua_tostring(lua,stack)))
		luaL_error(lua,"failed to create thread");
	return 1;
}
static int dsl_CreateThread(lua_State *lua){
	script_manager *sm;
	
	#ifdef DSL_SERVER_VERSION
	sm = getActiveManager(lua,1);
	#else
	sm = getActiveManager(lua,0);
	if(!sm || sm->use_base_funcs)
		return callGameLuaFunctionCreateThread(lua);
	#endif
	return createThreadOfType(lua,sm,GAME_THREAD,1);
}
#ifndef DSL_SERVER_VERSION
static int dsl_CreateSystemThread(lua_State *lua){
	return createThreadOfType(lua,getActiveManager(lua,1),SYSTEM_THREAD,1);
}
static int dsl_CreateDrawingThread(lua_State *lua){
	return createThreadOfType(lua,getActiveManager(lua,1),DRAWING_THREAD,1);
}
#endif
static int dsl_CreateAdvancedThread(lua_State *lua){
	const char *type;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	type = lua_tostring(lua,1);
	if(dslisenum(type,"PRE_GAME"))
		return createThreadOfType(lua,getActiveManager(lua,1),PRE_GAME_THREAD,2);
	if(dslisenum(type,"GAME"))
		return createThreadOfType(lua,getActiveManager(lua,1),GAME_THREAD,2);
	if(dslisenum(type,"GAME2"))
		return createThreadOfType(lua,getActiveManager(lua,1),GAME_THREAD2,2);
	if(dslisenum(type,"SYSTEM"))
		return createThreadOfType(lua,getActiveManager(lua,1),SYSTEM_THREAD,2);
	if(dslisenum(type,"DRAWING"))
		return createThreadOfType(lua,getActiveManager(lua,1),DRAWING_THREAD,2);
	if(dslisenum(type,"POST_WORLD"))
		return createThreadOfType(lua,getActiveManager(lua,1),POST_WORLD_THREAD,2);
	if(dslisenum(type,"PRE_FADE"))
		return createThreadOfType(lua,getActiveManager(lua,1),PRE_FADE_THREAD,2);
	luaL_argerror(lua,1,"unknown thread type");
	return 0;
}

// THREADS - CLEANUP
static int dsl_TerminateCurrentThread(lua_State *lua){
	script_manager *sm;
	
	sm = getActiveManager(lua,2);
	shutdownThread(sm->running_thread,lua,1);
	return 0;
}
static int dsl_TerminateThread(lua_State *lua){
	script_manager *sm;
	thread *t;
	
	#ifdef DSL_SERVER_VERSION
	sm = getActiveManager(lua,1);
	#else
	sm = getActiveManager(lua,0);
	if(!sm || sm->use_base_funcs || lua_type(lua,1) == LUA_TNUMBER)
		return callGameLuaFunctionTerminateThread(lua);
	#endif
	if(lua_gettop(lua)){
		luaL_checktype(lua,1,LUA_TTHREAD);
		if(t = getThreadFromThread(lua,1))
			shutdownThread(t,lua,1);
	}else if(sm = getActiveManager(lua,2))
		shutdownThread(sm->running_thread,lua,1);
	return 0;
}

// THREADS - QUERY
static int dsl_GetThreadName(lua_State *lua){
	script_manager *sm;
	thread *t;
	const char *name;
	
	if(lua_gettop(lua)){
		luaL_checktype(lua,1,LUA_TTHREAD);
		t = getThreadFromThread(lua,1);
		if(!t || t->flags & SHUTDOWN_THREAD)
			luaL_error(lua,"invalid thread");
		name = getThreadName(t);
	}else if(sm = getActiveManager(lua,2))
		name = getThreadName(sm->running_thread);
	if(!name)
		return 0;
	lua_pushstring(lua,name);
	return 1;
}
static int dsl_IsThreadRunning(lua_State *lua){
	thread *t;
	
	luaL_checktype(lua,1,LUA_TTHREAD);
	t = getThreadFromThread(lua,1);
	lua_pushboolean(lua,t && ~t->flags & SHUTDOWN_THREAD);
	return 1;
}

// THREADS - WAIT
static int dsl_GetThreadWait(lua_State *lua){
	script_manager *sm;
	thread *t;
	
	if(lua_gettop(lua)){
		luaL_checktype(lua,1,LUA_TTHREAD);
		t = getThreadFromThread(lua,1);
		if(!t || t->flags & SHUTDOWN_THREAD)
			luaL_error(lua,"invalid thread");
		lua_pushnumber(lua,t->when - getGameTimer());
	}else if(sm = getActiveManager(lua,2))
		lua_pushnumber(lua,sm->running_thread->when - getGameTimer());
	return 1;
}
static int dsl_Wait(lua_State *lua){
	script_manager *sm;
	
	#ifndef DSL_SERVER_VERSION
	sm = getActiveManager(lua,0);
	if(!sm || sm->use_base_funcs)
		return callGameLuaFunctionWait(lua);
	#endif
	sm = getActiveManager(lua,2);
	sm->running_thread->when = getGameTimer() + lua_tonumber(lua,1);
	return lua_yield(lua,0);
}

// MANAGER - MISCELLANEOUS
#ifndef DSL_SERVER_VERSION
static int dsl_UseBaseGameScriptFunctions(lua_State *lua){
	script_manager *sm;
	
	luaL_checktype(lua,1,LUA_TBOOLEAN);
	if(sm = getActiveManager(lua,0))
		sm->use_base_funcs = lua_toboolean(lua,1);
	return 0;
}
#endif

// DEBUG
#ifndef DSL_SERVER_VERSION
static int dsl_GetAllScriptInfo(lua_State *lua){
	script_manager *sm;
	script_collection *c;
	loader_collection *lc;
	script *s;
	unsigned i;
	int net;
	
	if(lua_gettop(lua))
		luaL_checktype(lua,1,LUA_TBOOLEAN);
	sm = getDslState(lua,1)->manager;
	net = lua_toboolean(lua,1);
	lua_newtable(lua);
	for(c = sm->collections;c;c = c->next)
		if(net == ((lc = c->lc) && lc->flags & LOADER_NETWORK)){
			lua_pushstring(lua,getScriptCollectionName(c));
			lua_newtable(lua);
			for(i = 0,s = c->scripts;s;s = s->next){
				lua_newtable(lua);
				lua_pushstring(lua,"name");
				lua_pushstring(lua,getScriptName(s));
				lua_rawset(lua,-3);
				if(s->flags & HASHED_SCRIPT){
					lua_pushstring(lua,"hash");
					lua_pushlightuserdata(lua,(void*)s->hash);
					lua_rawset(lua,-3);
				}
				lua_rawseti(lua,-2,++i);
			}
			lua_rawset(lua,-3);
		}
	return 1;
}
static int dsl_GetScriptObject(lua_State *lua){
	script *s;
	
	if(lua_gettop(lua)){
		luaL_checktype(lua,1,LUA_TUSERDATA);
		s = *(script**)lua_touserdata(lua,1);
	}else
		s = getDslState(lua,1)->manager->running_script;
	if(!s || s->flags & SHUTDOWN_SCRIPT)
		luaL_error(lua,"invalid script");
	lua_pushlstring(lua,s->script_object,SCRIPT_BYTES);
	return 1;
}
static int dsl_CompareNextScriptObject(lua_State *lua){
	const char *a;
	const char *b;
	char buf[12];
	int i;
	
	luaL_checktype(lua,1,LUA_TUSERDATA);
	luaL_checktype(lua,2,LUA_TNUMBER);
	a = lua_touserdata(lua,1);
	i = lua_tonumber(lua,2);
	b = a + SCRIPT_BYTES;
	if(i < 0)
		i = 0;
	else
		i += 4;
	for(;i < SCRIPT_BYTES;i++)
		if(a[i] != b[i]){
			lua_pushnumber(lua,i);
			i -= i % 4;
			sprintf(buf,"%.2hhX %.2hhX %.2hhX %.2hhX",(unsigned char)a[i],(unsigned char)a[i+1],(unsigned char)a[i+2],(unsigned char)a[i+3]);
			lua_pushlstring(lua,buf,sizeof(buf));
			sprintf(buf,"%.2hhX %.2hhX %.2hhX %.2hhX",(unsigned char)b[i],(unsigned char)b[i+1],(unsigned char)b[i+2],(unsigned char)b[i+3]);
			lua_pushlstring(lua,buf,sizeof(buf));
			return 3;
		}
	return 0;
}
static int dsl_CompareScriptObjects(lua_State *lua){
	char *str;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TSTRING);
	if(lua_strlen(lua,1) != SCRIPT_BYTES)
		luaL_argerror(lua,1,"invalid script object");
	if(lua_strlen(lua,2) != SCRIPT_BYTES)
		luaL_argerror(lua,2,"invalid script object");
	lua_pushcfunction(lua,&dsl_CompareNextScriptObject);
	str = lua_newuserdata(lua,SCRIPT_BYTES*2);
	memcpy(str,lua_tostring(lua,1),SCRIPT_BYTES);
	memcpy(str+SCRIPT_BYTES,lua_tostring(lua,2),SCRIPT_BYTES);
	lua_pushnumber(lua,-1);
	return 3;
}
#endif

// OPEN
#ifndef DSL_SERVER_VERSION
static int createdScript(lua_State *lua,void *arg,script *t){
	lua_pushstring(lua,"ImportScript");
	lua_pushcfunction(lua,&dsl_ImportScript);
	lua_rawset(lua,-3);
	return 0;
}
#endif
int dslopen_manager(lua_State *lua){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	#ifndef DSL_SERVER_VERSION
	addScriptEventCallback(dsl->events,EVENT_SCRIPT_CREATED,(script_event_cb)&createdScript,NULL);
	#endif
	// COLLECTIONS - GLOBAL SHARED
	lua_pushstring(lua,"dsl");
	lua_newtable(lua);
	lua_newtable(lua);
	lua_pushstring(lua,"__index");
	lua_pushcfunction(lua,&dsl_shared_index);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"__newindex");
	lua_pushcfunction(lua,&dsl_shared_newindex);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lua_settable(lua,LUA_GLOBALSINDEX);
	// COLLECTIONS - NETWORK SHARED
	lua_pushstring(lua,"net");
	lua_newtable(lua);
	lua_newtable(lua);
	lua_pushstring(lua,"__index");
	lua_pushcfunction(lua,&net_shared_index);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"__newindex");
	lua_pushcfunction(lua,&net_shared_newindex);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lua_settable(lua,LUA_GLOBALSINDEX);
	// SCRIPTS - HEADERS
	lua_register(lua,"AllowOnServer",&dsl_AllowOnServer); // ()                               <- allow this script collection to run on servers.
	lua_register(lua,"DontAutoStartScript",&dsl_DontAutoStartScript); // ()                   <- destroy the script script_collection immediately if it was started by the initial auto start.
	lua_register(lua,"RequireDependency",&dsl_RequireDependency); // ("name")                 <- start the dependency collection and destroy the script if it cannot be started.
	lua_register(lua,"RequireLoaderVersion",&dsl_RequireLoaderVersion); // (number)           <- destroy the script if the loader isn't at least this version.
	lua_register(lua,"RequireSystemAccess",&dsl_RequireSystemAccess); // ()                   <- destroy the script if system access is disabled in the config.
	// SCRIPTS - INVOCATION
	lua_register(lua,"LoadScript",&dsl_LoadScript); // ("name.lua")                           <- run a script in the running script's environment (like ImportScript).
	lua_register(lua,"StartScript",&dsl_StartScript); // ("name.lua")                         <- start a script given a relative file name, returns a script object.
	lua_register(lua,"StartVirtualScript",&dsl_StartVirtualScript); // (["name"],func)        <- start a script given a function.
	// SCRIPTS - CLEANUP
	lua_register(lua,"StopCurrentScriptCollection",&dsl_StopCurrentScriptCollection); // ()   <- stop the running script collection.
	lua_register(lua,"TerminateCurrentScript",&dsl_TerminateCurrentScript); // ()             <- stop the running script.
	lua_register(lua,"TerminateScript",&dsl_TerminateScript); // (script)                     <- stop a script (no arg = use running script).
	// SCRIPTS - QUERY
	lua_register(lua,"GetCurrentScript",&dsl_GetCurrentScript); // (script)                   <- get the current script object.
	lua_register(lua,"GetScriptCollection",&dsl_GetScriptCollection); // (script)             <- get a script's collection name (no arg = use running script).
	lua_register(lua,"GetScriptEnvironment",&dsl_GetScriptEnvironment); // (script)           <- get a script's environment (no arg = use running script).
	lua_register(lua,"GetScriptName",&dsl_GetScriptName); // (script)                         <- get a script's name (no arg = use running script).
	lua_register(lua,"GetScriptSharedTable",&dsl_GetScriptSharedTable); // (private)          <- if private is true, the shared table will not be available in the dsl table.
	lua_register(lua,"GetScriptNetworkTable",&dsl_GetScriptNetworkTable); // ()               <- this table is available in the net table.
	lua_register(lua,"IsScriptRunning",&dsl_IsScriptRunning); // (script)                     <- returns true if the script is active and not shutting down.
	lua_register(lua,"IsScriptZipped",&dsl_IsScriptZipped); // (script)                       <- returns true if the script's collection is a zip (no arg = use running script).
	// THREADS - CREATION
	lua_register(lua,"CreateThread",&dsl_CreateThread); // (func,...)                         <- create a game thread given a function and any amount of arguments, returns a thread object.
	#ifndef DSL_SERVER_VERSION
	lua_register(lua,"CreateSystemThread",&dsl_CreateSystemThread); // (func,...)             <- create a system thread.
	lua_register(lua,"CreateDrawingThread",&dsl_CreateDrawingThread); // (func,...)           <- create a drawing thread.
	#endif
	lua_register(lua,"CreateAdvancedThread",&dsl_CreateAdvancedThread); // (type,func,...)    <- create a thread given a type name.
	// THREADS - CLEANUP
	lua_register(lua,"TerminateCurrentThread",&dsl_TerminateCurrentThread); // ()             <- terminate the running thread.
	lua_register(lua,"TerminateThread",&dsl_TerminateThread); // (thread)                     <- terminate a thread (no arg = use running thread).
	// THREADS - QUERY
	lua_register(lua,"GetThreadName",&dsl_GetThreadName); // (thread)                         <- returns a thread's name (no arg = use running thread).
	lua_register(lua,"IsThreadRunning",&dsl_IsThreadRunning); // (thread)                     <- returns true if the thread is active and not shutting down.
	// THREADS - WAIT
	lua_register(lua,"GetThreadWait",&dsl_GetThreadWait); // ()                               <- help coroutines make use of Wait by returning the time that was used for Wait.
	lua_register(lua,"Wait",&dsl_Wait); // (ms)                                               <- yield the current thread for some amount of time (or 0 for one tick).
	// MANAGER - MISCELLANEOUS
	#ifndef DSL_SERVER_VERSION
	lua_register(lua,"UseBaseGameScriptFunctions",&dsl_UseBaseGameScriptFunctions); // (bool) <- make the replaced game functions use the original versions for this thread update.
	// DEBUG
	if(dsl->flags & DSL_ADD_DEBUG_FUNCS){
		lua_register(lua,"GetAllScriptInfo",&dsl_GetAllScriptInfo);
		lua_register(lua,"GetScriptObject",&dsl_GetScriptObject);
		lua_register(lua,"CompareScriptObjects",&dsl_CompareScriptObjects);
	}
	#endif
	return 0;
}