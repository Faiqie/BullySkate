// DERPY'S SCRIPT LOADER: HOOK LIBRARY

#include <dsl/dsl.h>
#include <string.h>

#define REGISTRY_KEY DSL_REGISTRY_KEY "_hook"
#define INCREMENT_COUNT 32
#define MAX_HOOKS 200

#define NOT_REPLACED 0
#define REPLACED_NORMAL 1
#define REPLACED_EXCLUSIVE 2

//#define USE_SCRIPT_BLOCKS

// types
typedef struct lua_library{
	const char *name;
	lua_CFunction func;
}lua_library;
typedef struct func_replacement{
	lua_CFunction func; // NULL if not replaced and no hooks
	int replaced;
	script *s; // only valid if replaced
	int hooks[MAX_HOOKS];
	script *hs[MAX_HOOKS]; // only valid when the hook in the same position isnt LUA_NOREF
	int hooklimit; // +1 of the highest hook position that was ever valid
}func_replacement;
typedef struct func_replacements{
	func_replacement **array;
	size_t index;
	size_t count;
}func_replacements;

// natives
static const lua_library *g_libraries[] = {
	(lua_library*)0xAE9DA8,
	(lua_library*)0xAEAFA8,
	(lua_library*)0xAEA2C8,
	(lua_library*)0xAE9AA8,
	(lua_library*)0xAE96F8,
	(lua_library*)0xAE9418,
	(lua_library*)0xAE9530,
	(lua_library*)0xAE9100,
	(lua_library*)0xAE9958,
	(lua_library*)0xAEB510,
	(lua_library*)0xAEB200,
	(lua_library*)0xAEB770,
	(lua_library*)0xAEB6A8,
	(lua_library*)0xAEA580,
	(lua_library*)0xAE9B20,
	(lua_library*)0xAE9890,
	(lua_library*)0xAEA28C,
	(lua_library*)0xAE9D40,
	(lua_library*)0xAEB530,
	(lua_library*)0xAE9990,
	(lua_library*)0xAEA310,
	(lua_library*)0xAEAF38,
	(lua_library*)0xAEA3C8,
	(lua_library*)0xAEB240,
	(lua_library*)0xAE9F78,
	(lua_library*)0xAE9918,
	(lua_library*)0xAE94E0,
	(lua_library*)0xAEB2B8,
	(lua_library*)0xB9DA18,
	(lua_library*)0xBC6ED8,
	(lua_library*)0xAE9A78,
	(lua_library*)0xAE9518,
	(lua_library*)0xBC6598,
	(lua_library*)0xBC6700,
	(lua_library*)0xBC6660,
	(lua_library*)0xBC63D8,
	(lua_library*)0xBC64F8,
	(lua_library*)0xBC6298,
	(lua_library*)0xBC6040,
	(lua_library*)0xBC5F54,
	(lua_library*)0xBC6AF0,
	(lua_library*)0xBC6C4C,
	(lua_library*)0xBC6FF8,
	(lua_library*)0xBC7170,
	(lua_library*)0xBC5FAC,
	(lua_library*)0xBC7068,
	(lua_library*)0xBC68E8,
	(lua_library*)0xBC6008,
	(lua_library*)0xBC6B5C,
	(lua_library*)0xBC6890,
	(lua_library*)0xAE9B90,
	(lua_library*)0xAEB674,
	(lua_library*)0xAE9778,
	(lua_library*)0xAEA25C,
	(lua_library*)0xAE9858,
	(lua_library*)0xAEB748,
	(lua_library*)0xAE9A28,
	(lua_library*)0xAEB5C8
};

// utility
static lua_CFunction getLuaFunction(const char *name){
	const lua_library *lib;
	int count;
	
	count = sizeof(g_libraries) / sizeof(lua_library*);
	while(count)
		for(lib = g_libraries[--count];lib->name;lib++)
			if(!strcmp(lib->name,name))
				return lib->func;
	return NULL;
}
static const char* getLuaFunctionName(lua_CFunction func){
	const lua_library *lib;
	int count;
	
	count = sizeof(g_libraries) / sizeof(lua_library*);
	while(count)
		for(lib = g_libraries[--count];lib->name;lib++)
			if(lib->func == func)
				return lib->name;
	return NULL;
}
static func_replacement* getReplacement(lua_State *lua,lua_CFunction func){
	func_replacements *fr;
	func_replacement *ptr;
	size_t resize;
	int i;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	fr = lua_touserdata(lua,-1);
	lua_pop(lua,1);
	i = fr->index;
	while(i){
		ptr = fr->array[--i];
		if(!ptr->func || ptr->func == func)
			return ptr;
	}
	if(fr->index == fr->count){
		resize = fr->count + INCREMENT_COUNT;
		ptr = realloc(fr->array,resize*sizeof(func_replacement*));
		if(!ptr)
			luaL_error(lua,"failed to replace function");
		fr->array = (func_replacement**)ptr;
		fr->count = resize;
	}
	ptr = malloc(sizeof(func_replacement)); // this'll exist until Lua is closed
	if(!ptr)
		luaL_error(lua,"failed to replace function");
	ptr->func = func;
	ptr->replaced = NOT_REPLACED;
	for(i = 0;i < MAX_HOOKS;i++)
		ptr->hooks[i] = LUA_NOREF;
	ptr->hooklimit = 0;
	return fr->array[fr->index++] = ptr;
}
static int isReplacementNeeded(func_replacement *ptr){
	int i;
	
	if(ptr->replaced)
		return 1;
	for(i = 0;i < MAX_HOOKS;i++)
		if(ptr->hooks[i] != LUA_NOREF)
			return 1;
	return 0;
}
static void printFailure(lua_State *lua){
	dsl_state *dsl;
	
	if(dsl = getDslState(lua,0)){
		if(lua_isstring(lua,-1))
			printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),lua_tostring(lua,-1));
		else
			printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),TEXT_FAIL_UNKNOWN);
	}
	lua_pop(lua,1);
}

// functions
static int replacedFunction(lua_State *lua){
	func_replacement *ptr;
	#ifdef USE_SCRIPT_BLOCKS
	script_manager *sm;
	script_block sb;
	#endif
	int argbase;
	int stack;
	int i;
	
	ptr = lua_touserdata(lua,lua_upvalueindex(1));
	lua_pushvalue(lua,lua_upvalueindex(2));
	if(ptr->func && lua_isfunction(lua,-1)){
		lua_insert(lua,1); // the function we're gonna call
		lua_pushcfunction(lua,ptr->func);
		lua_insert(lua,2); // the original function (passed as an arg)
		stack = lua_gettop(lua);
		argbase = 3;
	}else{
		lua_pop(lua,1);
		lua_pushcfunction(lua,ptr->func);
		lua_insert(lua,1);
		stack = lua_gettop(lua);
		argbase = 2;
	}
	if(ptr->func && ptr->hooklimit){
		if(!lua_checkstack(lua,stack+LUA_MINSTACK))
			luaL_error(lua,"failed to prepare hooks");
		lua_newtable(lua); // args
		i = argbase;
		while(i <= stack){
			lua_pushvalue(lua,i++);
			lua_rawseti(lua,-2,i-argbase);
		}
		lua_pushstring(lua,"n");
		lua_pushnumber(lua,stack-argbase+1); // without the replacement/original functions or args table
		lua_rawset(lua,-3);
		lua_insert(lua,1); // put in front of everything else
	}
	#ifdef USE_SCRIPT_BLOCKS
	sm = getDslState(lua,1)->manager;
	startScriptBlock(sm,ptr->s,&sb);
	#endif
	if(lua_pcall(lua,stack-1,LUA_MULTRET,0)) // replacement(original,...)
		printFailure(lua);
	#ifdef USE_SCRIPT_BLOCKS
	finishScriptBlock(sm,&sb,lua);
	#endif
	stack = lua_gettop(lua);
	if(ptr->func && ptr->hooklimit){
		if(!lua_checkstack(lua,stack+LUA_MINSTACK))
			luaL_error(lua,"failed to run hooks");
		lua_newtable(lua); // results
		for(i = 2;i <= stack;i++){
			lua_pushvalue(lua,i);
			lua_rawseti(lua,-2,i-1);
		}
		stack--; // decrement stack to account for the args table
		lua_pushstring(lua,"n");
		lua_pushnumber(lua,stack);
		lua_rawset(lua,-3);
		for(i = 0;i < ptr->hooklimit;i++)
			if(ptr->hooks[i] != LUA_NOREF){
				lua_rawgeti(lua,LUA_REGISTRYINDEX,ptr->hooks[i]);
				lua_pushvalue(lua,1); // args
				lua_pushvalue(lua,-3); // results
				lua_pushboolean(lua,argbase == 3); // ran replacement function
				#ifdef USE_SCRIPT_BLOCKS
				startScriptBlock(sm,ptr->hs[i],&sb);
				#endif
				if(lua_pcall(lua,3,0,0)) // hook(args,results,replaced)
					printFailure(lua);
				#ifdef USE_SCRIPT_BLOCKS
				finishScriptBlock(sm,&sb,lua);
				#endif
			}
		lua_pop(lua,1); // pop results table (leaving args is fine because it's before the real results)
	}
	return stack;
}
static int GetBaseGameFunction(lua_State *lua){
	lua_CFunction func;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	func = getLuaFunction(lua_tostring(lua,1));
	if(!func)
		return 0;
	lua_pushcfunction(lua,func);
	return 1;
}
static int HookFunction(lua_State *lua){
	func_replacement *ptr;
	lua_CFunction func;
	const char *name;
	int *result;
	int i;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TFUNCTION);
	lua_settop(lua,2);
	name = lua_tostring(lua,1);
	func = getLuaFunction(name);
	if(!func)
		luaL_argerror(lua,1,"incompatible function");
	ptr = getReplacement(lua,func);
	for(i = 0;i < MAX_HOOKS;i++)
		if(ptr->hooks[i] == LUA_NOREF)
			break;
	if(i == MAX_HOOKS)
		luaL_error(lua,"hook limit reached");
	if(i >= ptr->hooklimit)
		ptr->hooklimit = i + 1;
	result = (int*)!isReplacementNeeded(ptr); // just using result because no need for another variable
	ptr->hs[i] = getDslState(lua,1)->manager->running_script;
	ptr->hooks[i] = luaL_ref(lua,LUA_REGISTRYINDEX);
	if(result){
		lua_pushvalue(lua,1);
		lua_pushlightuserdata(lua,ptr);
		lua_pushnil(lua);
		lua_pushcclosure(lua,&replacedFunction,2);
		lua_settable(lua,LUA_GLOBALSINDEX);
	}
	result = lua_newuserdata(lua,sizeof(int)+strlen(name)+1);
	*result = 1;
	strcpy((char*)(result+1),name);
	return 1;
}
static int RemoveFunctionHook(lua_State *lua){
	func_replacement *ptr;
	lua_CFunction func;
	int *index;
	
	index = lua_touserdata(lua,1);
	if(!index)
		luaL_typerror(lua,1,"function hook");
	if(*index < 0 || *index >= MAX_HOOKS || !(func = getLuaFunction((char*)(index+1))) || !(ptr = getReplacement(lua,func)))
		luaL_error(lua,"corrupted function hook");
	if(ptr->hooks[*index] != LUA_NOREF || ptr->hs[*index] != getDslState(lua,1)->manager->running_script)
		luaL_argerror(lua,1,"invalid function hook");
	luaL_unref(lua,LUA_REGISTRYINDEX,ptr->hooks[*index]);
	ptr->hooks[*index] = LUA_NOREF;
	if(!isReplacementNeeded(ptr)){
		lua_pushstring(lua,(char*)(index+1));
		lua_pushcfunction(lua,func);
		lua_settable(lua,LUA_GLOBALSINDEX);
	}
	return 0;
}
static int ReplaceFunction(lua_State *lua){
	func_replacement *ptr;
	lua_CFunction func;
	script *s;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	if(lua_gettop(lua) >= 2 && !lua_isnil(lua,2))
		luaL_checktype(lua,2,LUA_TFUNCTION);
	if(lua_gettop(lua) >= 3)
		luaL_checktype(lua,3,LUA_TBOOLEAN);
	func = getLuaFunction(lua_tostring(lua,1));
	if(!func)
		luaL_argerror(lua,1,"incompatible function");
	s = getDslState(lua,1)->manager->running_script;
	ptr = getReplacement(lua,func);
	if(ptr->replaced != REPLACED_EXCLUSIVE || ptr->s == s){
		ptr->s = s;
		if(lua_isfunction(lua,2))
			ptr->replaced = lua_toboolean(lua,3) ? REPLACED_EXCLUSIVE : REPLACED_NORMAL;
		else
			ptr->replaced = NOT_REPLACED;
		if(!isReplacementNeeded(ptr))
			ptr->func = NULL;
		lua_pushvalue(lua,1);
		if(ptr->func){
			lua_pushlightuserdata(lua,ptr);
			lua_pushvalue(lua,2);
			lua_pushcclosure(lua,&replacedFunction,2);
		}else
			lua_pushcfunction(lua,func);
		lua_settable(lua,LUA_GLOBALSINDEX);
		lua_pushboolean(lua,1);
	}else
		lua_pushboolean(lua,0); // failed because the replacement was exclusive
	return 1;
}

// main
static int freeReplacements(lua_State *lua){
	func_replacements *fr;
	int i;
	
	fr = lua_touserdata(lua,1);
	i = fr->index;
	while(i)
		free(fr->array[--i]);
	free(fr->array);
	// not worried about the hook references because this is only free'd when lua is about to be free'd anyway
	return 0;
}
static func_replacements* initReplacements(lua_State *lua){
	func_replacements *fr;
	
	lua_pushstring(lua,REGISTRY_KEY);
	fr = lua_newuserdata(lua,sizeof(func_replacements));
	memset(fr,0,sizeof(func_replacements));
	lua_newtable(lua);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeReplacements);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lua_rawset(lua,LUA_REGISTRYINDEX);
	return fr;
}
static int destroyedScript(lua_State *lua,func_replacements *fr,script *s){
	func_replacement *ptr;
	func_replacement *finish;
	int index;
	int i;
	
	index = fr->index;
	while(index)
		if((ptr = fr->array[--index])->func){
			if(ptr->s == s)
				ptr->replaced = NOT_REPLACED;
			for(i = 0;i < MAX_HOOKS;i++)
				if(ptr->hs[i] == s && ptr->hooks[i] != LUA_NOREF){
					luaL_unref(lua,LUA_REGISTRYINDEX,ptr->hooks[i]);
					ptr->hooks[i] = LUA_NOREF;
				}
			if(!isReplacementNeeded(ptr)){
				lua_pushstring(lua,getLuaFunctionName(ptr->func)); // this will return a valid value because we already know func is valid
				lua_pushcfunction(lua,ptr->func);
				lua_settable(lua,LUA_GLOBALSINDEX);
			}else if(ptr->s == s){ // just removed the replacement function
				lua_pushstring(lua,getLuaFunctionName(ptr->func));
				lua_pushlightuserdata(lua,ptr);
				lua_pushnil(lua);
				lua_pushcclosure(lua,&replacedFunction,2);
				lua_settable(lua,LUA_GLOBALSINDEX);
			}
		}
	return 0;
}
int dslopen_hook(lua_State *lua){
	addScriptEventCallback(getDslState(lua,1)->events,EVENT_SCRIPT_CLEANUP,&destroyedScript,initReplacements(lua));
	lua_register(lua,"GetBaseGameFunction",&GetBaseGameFunction);
	lua_register(lua,"HookFunction",&HookFunction);
	lua_register(lua,"RemoveFunctionHook",&RemoveFunctionHook);
	lua_register(lua,"ReplaceFunction",&ReplaceFunction);
	return 0;
}