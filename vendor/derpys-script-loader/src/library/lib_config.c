// DERPY'S SCRIPT LOADER: CONFIG LIBRARY

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>

//#define ALLOW_INDEXING

// Lua type.
struct lua_cfg{
	void *cfg;
	int fail;
};

// String parser.
static int parseString(lua_State *lua,const char *value){
	char *copy,*str,c;
	int i,count;
	
	// strip double quotes and push strings that are seperated by commas
	copy = malloc(strlen(value) + 1);
	if(!copy)
		return 0; // so unlikely that we'll just say there are no results
	str = copy;
	while(c = *value++)
		if(c != '"')
			*str++ = c;
	*str = 0;
	str = copy;
	i = 0;
	count = 0;
	while(c = str[i]){
		if(c == ','){
			str[i] = 0;
			lua_pushstring(lua,str);
			str += i + 1;
			i = 0;
			while(isspace(*str))
				str++;
			count++;
		}else
			i++;
	}
	if(*str){
		lua_pushstring(lua,str);
		count++;
	}
	free(copy);
	return count;
}

// Load config.
static int getConfigFileString(lua_State *lua){
	lua_pushfstring(lua,"config: %s",(char*)((struct lua_cfg*)lua_touserdata(lua,1)+1));
	return 1;
}
#ifdef ALLOW_INDEXING
static int getConfigFileValue2(lua_State *lua){
	struct lua_cfg *ptr;
	const char *key;
	const char *value;
	int i;
	
	if(lua_type(lua,2) != LUA_TSTRING)
		return 0;
	ptr = lua_touserdata(lua,lua_upvalueindex(1));
	if(!ptr->cfg)
		return 0;
	key = lua_tostring(lua,2);
	value = getConfigValue(ptr->cfg,key);
	if(!value)
		return 0;
	switch((int)lua_tonumber(lua,lua_upvalueindex(2))){
		case 1:
			lua_pushboolean(lua,getConfigBoolean(ptr->cfg,key));
			return 1;
		case 2:
			i = 0;
			while(isdigit(value[i]))
				if(value[++i] == '.'){
					lua_pushnumber(lua,strtod(value,NULL));
					return 1;
				}
			lua_pushnumber(lua,strtol(value,NULL,0));
			return 1;
		case 3:
			lua_pushstring(lua,getConfigString(ptr->cfg,key));
			return 1;
	}
	lua_pushstring(lua,value);
	return 1;
}
static int getConfigFileValue(lua_State *lua){
	const char *what;
	int type;
	
	if(lua_type(lua,2) != LUA_TSTRING)
		return 0;
	what = lua_tostring(lua,2);
	if(!strcmp(what,"boolean"))
		type = 1;
	else if(!strcmp(what,"number"))
		type = 2;
	else if(!strcmp(what,"string"))
		type = 3;
	else if(!strcmp(what,"value"))
		type = 4;
	else
		type = 0;
	if(!type)
		return 0;
	lua_newtable(lua);
	lua_newtable(lua);
	lua_pushstring(lua,"__index");
	lua_pushvalue(lua,1);
	lua_pushnumber(lua,type);
	lua_pushcclosure(lua,&getConfigFileValue2,2);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	return 1;
}
#endif
static int unloadConfigFile(lua_State *lua){
	struct lua_cfg *ptr;
	
	ptr = lua_touserdata(lua,1);
	if(ptr->cfg)
		freeConfigSettings(ptr->cfg);
	return 0;
}
static int LoadConfigFile(lua_State *lua){
	script_collection *c;
	struct lua_cfg *ptr;
	const char *name;
	int reason;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	name = lua_tostring(lua,1);
	ptr = lua_newuserdata(lua,sizeof(struct lua_cfg)+strlen(name)+1);
	ptr->cfg = NULL;
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getConfigFileString);
	lua_rawset(lua,-3);
	#ifdef ALLOW_INDEXING
	lua_pushstring(lua,"__index");
	lua_pushcfunction(lua,&getConfigFileValue);
	lua_rawset(lua,-3);
	#endif
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&unloadConfigFile);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	strcpy((char*)(ptr+1),name);
	if(!(ptr->cfg = loadConfigSettingsEx(getDslState(lua,1),NULL,name,&reason))){
		if(reason == CONFIG_FAILURE)
			luaL_error(lua,"failed to load config"); // only for memory issues so basically never
		ptr->fail = reason;
	}
	return 1;
}
static int GetScriptConfig(lua_State *lua){
	loader_collection *lc;
	script_collection *c;
	struct lua_cfg *ptr;
	
	c = getDslState(lua,1)->manager->running_collection;
	if(c && (lc = c->lc) && lc->sc_cfg_ref != LUA_NOREF){
		lua_rawgeti(lua,LUA_REGISTRYINDEX,lc->sc_cfg_ref);
		if(!lua_isuserdata(lua,-1))
			luaL_error(lua,"corrupted config");
		return 1;
	}
	ptr = lua_newuserdata(lua,sizeof(struct lua_cfg)+sizeof(DSL_SCRIPT_CONFIG));
	ptr->cfg = NULL;
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getConfigFileString);
	lua_rawset(lua,-3);
	#ifdef ALLOW_INDEXING
	lua_pushstring(lua,"__index");
	lua_pushcfunction(lua,&getConfigFileValue);
	lua_rawset(lua,-3);
	#endif
	lua_setmetatable(lua,-2);
	strcpy((char*)(ptr+1),DSL_SCRIPT_CONFIG);
	if(c && lc){
		ptr->cfg = lc->sc_cfg;
		lua_pushvalue(lua,-1);
		lc->sc_cfg_ref = luaL_ref(lua,LUA_REGISTRYINDEX); // this userdata will now be associated with the loader collection until sc dies
	}
	if(!ptr->cfg)
		ptr->fail = CONFIG_MISSING;
	return 1;
}
static int IsConfigMissing(lua_State *lua){
	struct lua_cfg *ptr;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	lua_pushboolean(lua,!ptr->cfg && ptr->fail == CONFIG_MISSING);
	return 1;
}

// Get basic config.
static int GetConfigFileValue(lua_State *lua){
	struct lua_cfg *ptr;
	const char *value;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	if(value = getConfigValue(ptr->cfg,lua_tostring(lua,2)))
		lua_pushstring(lua,value);
	else if(lua_gettop(lua) < 3)
		return 0;
	else
		lua_settop(lua,3);
	return 1;
}
static int GetConfigFileNumber(lua_State *lua){
	struct lua_cfg *ptr;
	const char *value;
	const char *check;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	value = getConfigValue(ptr->cfg,lua_tostring(lua,2));
	if(!value || !isdigit(*value)){
		if(lua_gettop(lua) < 3)
			return 0;
		lua_settop(lua,3);
		return 1;
	}
	check = value; // check for a '.'
	while(isdigit(*check))
		if(*(++check) == '.'){
			lua_pushnumber(lua,strtod(value,NULL));
			return 1;
		}
	lua_pushnumber(lua,strtol(value,NULL,0)); // we use this if there's no decimal to support hex/octal
	return 1;
}
static int GetConfigFileBoolean(lua_State *lua){
	struct lua_cfg *ptr;
	const char *key;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	if(!getConfigValue(ptr->cfg,key = lua_tostring(lua,2))){
		if(lua_gettop(lua) < 3)
			return 0;
		lua_settop(lua,3);
		return 1;
	}
	lua_pushboolean(lua,getConfigBoolean(ptr->cfg,key));
	return 1;
}
static int GetConfigFileString(lua_State *lua){
	struct lua_cfg *ptr;
	const char *value;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	if(value = getConfigString(ptr->cfg,lua_tostring(lua,2)))
		lua_pushstring(lua,value);
	else if(lua_gettop(lua) < 3)
		return 0;
	else
		lua_settop(lua,3);
	return 1;
}
static int GetConfigFileStrings(lua_State *lua){
	struct lua_cfg *ptr;
	const char *value;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	value = getConfigValue(ptr->cfg,lua_tostring(lua,2));
	if(!value)
		return lua_gettop(lua) - 2;
	return parseString(lua,value);
}

// List array config.
static int iterConfigFileValues(lua_State *lua){
	const char *key;
	void *cfg;
	int index;
	
	cfg = ((struct lua_cfg*)lua_touserdata(lua,lua_upvalueindex(1)))->cfg;
	key = lua_tostring(lua,lua_upvalueindex(2));
	index = lua_tonumber(lua,lua_upvalueindex(3));
	key = getConfigValueArray(cfg,key,index);
	if(!key)
		return 0;
	lua_pushnumber(lua,index+1);
	lua_replace(lua,lua_upvalueindex(3));
	lua_pushstring(lua,key);
	return 1;
}
static int AllConfigFileValues(lua_State *lua){
	struct lua_cfg *ptr;
	const char *key;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	lua_settop(lua,2);
	lua_pushnumber(lua,0);
	lua_pushcclosure(lua,&iterConfigFileValues,3);
	return 1;
}
static int iterConfigFileStrings(lua_State *lua){
	const char *key;
	void *cfg;
	int index;
	
	cfg = ((struct lua_cfg*)lua_touserdata(lua,lua_upvalueindex(1)))->cfg;
	key = lua_tostring(lua,lua_upvalueindex(2));
	index = lua_tonumber(lua,lua_upvalueindex(3));
	key = getConfigStringArray(cfg,key,index);
	if(!key)
		return 0;
	lua_pushnumber(lua,index+1);
	lua_replace(lua,lua_upvalueindex(3));
	lua_pushstring(lua,key);
	return 1;
}
static int AllConfigFileStrings(lua_State *lua){ // parsed version
	struct lua_cfg *ptr;
	const char *key;
	
	ptr = lua_touserdata(lua,1);
	if(!ptr)
		luaL_typerror(lua,1,"config");
	luaL_checktype(lua,2,LUA_TSTRING);
	lua_settop(lua,2);
	lua_pushnumber(lua,0);
	lua_pushcclosure(lua,&iterConfigFileStrings,3);
	return 1;
}

// Cleanup collection cfg.
static int destroyedCollection(lua_State *lua,void *arg,script_collection *sc){
	loader_collection *lc;
	struct lua_cfg *ptr;
	
	lc = sc->lc;
	if(lc && lc->sc_cfg_ref != LUA_NOREF){
		lua_rawgeti(lua,LUA_REGISTRYINDEX,lc->sc_cfg_ref);
		if(ptr = lua_touserdata(lua,-1)){
			ptr->cfg = NULL;
			ptr->fail = CONFIG_MISSING;
		}
		lua_pop(lua,1);
		luaL_unref(lua,LUA_REGISTRYINDEX,lc->sc_cfg_ref);
		lc->sc_cfg_ref = LUA_NOREF;
	}
	return 0;
}

// Register lua functions.
int dslopen_config(lua_State *lua){
	addScriptEventCallback(getDslState(lua,1)->events,EVENT_COLLECTION_DESTROYED,&destroyedCollection,NULL);
	lua_register(lua,"LoadConfigFile",&LoadConfigFile); // config (string file*)
	lua_register(lua,"GetScriptConfig",&GetScriptConfig); // config ()
	lua_register(lua,"IsConfigMissing",&IsConfigMissing); // boolean (config config)
	lua_register(lua,"GetConfigValue",&GetConfigFileValue); // string|any (config config, string key, any default)
	lua_register(lua,"GetConfigNumber",&GetConfigFileNumber); // number|any (config config, string key, any default)
	lua_register(lua,"GetConfigBoolean",&GetConfigFileBoolean); // boolean|any (config config, string key, any default)
	lua_register(lua,"GetConfigString",&GetConfigFileString); // string|any (config config, string key, any default)
	lua_register(lua,"GetConfigStrings",&GetConfigFileStrings); // string...|any... (config config, string key, any default...)
	lua_register(lua,"AllConfigValues",&AllConfigFileValues); // iterator (config config, string key)
	lua_register(lua,"AllConfigStrings",&AllConfigFileStrings); // iterator (config config, string key)
	return 0;
}