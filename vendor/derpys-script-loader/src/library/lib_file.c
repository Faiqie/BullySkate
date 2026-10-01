// DERPY'S SCRIPT LOADER: FILE LIBRARY

#include <dsl/dsl.h>
#include <string.h>

#define MAX_REQUIRE_PATH_LENGTH 100

// UTILITY
static dsl_file* getFile(lua_State *lua,int reqstd){
	dsl_file *file;
	
	if(!lua_isuserdata(lua,1))
		luaL_typerror(lua,1,"file");
	file = *(dsl_file**)lua_touserdata(lua,1);
	if(!file)
		luaL_argerror(lua,1,"closed file");
	if(reqstd && isDslFileInZip(file))
		luaL_argerror(lua,1,"zipped file");
	return file;
}

// BASICS
static int CanWriteFiles(lua_State *lua){
	lua_pushboolean(lua,~getDslState(lua,1)->flags & DSL_CONNECTED_TO_SERVER);
	return 1;
}
static int GetScriptFilePath(lua_State *lua){
	script_collection *sc;
	loader_collection *lc;
	
	if(lua_gettop(lua))
		luaL_checktype(lua,1,LUA_TSTRING);
	if(sc = getDslState(lua,1)->manager->running_collection){
		lc = sc->lc;
		if(!lc)
			luaL_error(lua,"failed to get current script collection path");
		if(lc->zip){
			if(lua_gettop(lua) >= 2)
				luaL_error(lua,"cannot get file path inside zipped collection");
			lua_pushfstring(lua,"%s%s",lc->prefix,(char*)(lc+1));
			return 1;
		}
		if(lc->flags & LOADER_FOLDER)
			lua_pushfstring(lua,"%s%s/",lc->prefix,(char*)(lc+1));
		else
			lua_pushstring(lua,lc->prefix);
	}else
		lua_pushstring(lua,"Scripts/");
	if(lua_gettop(lua) >= 2){
		lua_settop(lua,2);
		lua_insert(lua,1);
		lua_concat(lua,2);
	}
	return 1;
}
static int GetPackageFilePath(lua_State *lua){
	lua_pushstring(lua,DSL_PACKAGE_PATH);
	if(lua_gettop(lua) > 1){
		luaL_checktype(lua,1,LUA_TSTRING);
		lua_insert(lua,-2);
		lua_concat(lua,2);
	}
	return 1;
}
static int IsSystemAccessAllowed(lua_State *lua){
	lua_pushboolean(lua,getDslState(lua,1)->flags & DSL_SYSTEM_ACCESS);
	return 1;
}

// STANDARD
static int dsl_dofile(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	if(!isDslFileSafe(getDslState(lua,1),lua_tostring(lua,1)))
		luaL_argerror(lua,1,"forbidden file path");
	if(luaL_loadfile(lua,lua_tostring(lua,-1)))
		lua_error(lua);
	lua_replace(lua,1);
	lua_settop(lua,1);
	lua_call(lua,0,LUA_MULTRET);
	return lua_gettop(lua);
}
static int dsl_loadfile(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	if(!isDslFileSafe(getDslState(lua,1),lua_tostring(lua,1)))
		lua_pushstring(lua,"forbidden file path");
	else if(!luaL_loadfile(lua,lua_tostring(lua,-1)))
		return 1;
	lua_pushnil(lua);
	lua_insert(lua,-2);
	return 2;
}
static int dsl_require(lua_State *lua){
	char buffer[sizeof(DSL_PACKAGE_PATH) + MAX_REQUIRE_PATH_LENGTH + 4]; // + 4 for file extension (a NUL comes from the sizeof path)
	const char *ptr;
	int attempt;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	lua_pushstring(lua,"_LOADED");
	lua_gettable(lua,LUA_GLOBALSINDEX);
	if(!lua_istable(lua,-1))
		luaL_error(lua,"`_LOADED' is not a table");
	lua_pushvalue(lua,1);
	lua_rawget(lua,-2); // get _LOADED[package]
	if(lua_toboolean(lua,-1))
		return 1; // already loaded result
	lua_pop(lua,1);
	ptr = lua_tostring(lua,1);
	if(strlen(ptr) > MAX_REQUIRE_PATH_LENGTH)
		luaL_argerror(lua,1,"package name is too long");
	strcpy(buffer,DSL_PACKAGE_PATH);
	strcat(buffer,ptr);
	for(attempt = 0;attempt < 3;attempt++){
		if(attempt == 1)
			strcat(buffer,".lua");
		else if(attempt == 2)
			buffer[strlen(buffer)-1] = 'r';
		if(!isDslFileSafe(getDslState(lua,1),buffer))
			luaL_argerror(lua,1,"forbidden file path");
		if(!luaL_loadfile(lua,buffer)){
			lua_pushstring(lua,"_REQUIREDNAME");
			lua_pushstring(lua,ptr);
			lua_settable(lua,LUA_GLOBALSINDEX);
			lua_call(lua,0,1);
			if(lua_isnil(lua,-1)){
				lua_pop(lua,1);
				lua_pushboolean(lua,1);
			}
			lua_pushvalue(lua,1);
			lua_pushvalue(lua,-2);
			lua_rawset(lua,-4); // _LOADED[package] = result
			return 1; // return result
		}
		lua_pop(lua,1); // pop error from not loading file
	}
	luaL_error(lua,"could not load package `%s' from `%s?.lua'",ptr,DSL_PACKAGE_PATH);
	return 0;
}

// RAW I/O
static int getFileString(lua_State *lua){
	dsl_file **ptr;
	
	if(ptr = lua_touserdata(lua,1))
		lua_pushfstring(lua,"file: %s",(char*)(ptr+1));
	else
		lua_pushstring(lua,"invalid file");
	return 1;
}
static int gcFile(lua_State *lua){
	dsl_file *file;
	
	if(file = *(dsl_file**)lua_touserdata(lua,1))
		closeDslFile(file);
	return 0;
}
static int dsl_OpenFile(lua_State *lua){
	const char *name,*mode;
	dsl_file *file,**ptr;
	dsl_state *dsl;
	long size;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TSTRING);
	lua_setgcthreshold(lua,0); // close any invalid files
	name = lua_tostring(lua,1);
	mode = lua_tostring(lua,2);
	if(!dslstrcmp(mode,"a") || !dslstrcmp(mode,"ab"))
		mode = "ab";
	else if(!dslstrcmp(mode,"r") || !dslstrcmp(mode,"rb"))
		mode = "rb";
	else if(!dslstrcmp(mode,"w") || !dslstrcmp(mode,"wb"))
		mode = "wb";
	else
		luaL_argerror(lua,2,"invalid access mode");
	dsl = getDslState(lua,1);
	if(*mode != 'r' && dsl->flags & DSL_CONNECTED_TO_SERVER)
		luaL_error(lua,"cannot open files for writing after connecting to a server");
	ptr = lua_newuserdata(lua,sizeof(dsl_file**)+strlen(name)+1);
	*ptr = NULL;
	strcpy((char*)(ptr+1),name);
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getFileString);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&gcFile);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	if(*mode == 'w'){
		size = 0;
		file = openDslFile(dsl,NULL,name,mode,NULL);
	}else
		file = openDslFile(dsl,NULL,name,mode,&size);
	if(!file)
		luaL_argerror(lua,1,lua_pushfstring(lua,"%s: %s",getDslFileError(),name));
	*ptr = file;
	lua_pushnumber(lua,size);
	return 2;
}
static int dsl_CloseFile(lua_State *lua){
	dsl_file **ptr;
	
	ptr = lua_touserdata(lua,1);
	closeDslFile(getFile(lua,0));
	*ptr = NULL;
	return 0;
}
static int dsl_FlushFile(lua_State *lua){
	if(fflush(getFile(lua,1)->file) == EOF)
		luaL_error(lua,"failed to flush file");
	return 0;
}
static int dsl_SeekFile(lua_State *lua){
	FILE *file;
	int offset;
	int origin;
	const char *str;
	
	file = getFile(lua,1)->file;
	offset = luaL_checknumber(lua,2);
	if(str = lua_tostring(lua,3)){
		if(dslisenum(str,"SEEK_SET"))
			origin = SEEK_SET;
		else if(dslisenum(str,"SEEK_CUR"))
			origin = SEEK_CUR;
		else if(dslisenum(str,"SEEK_END"))
			origin = SEEK_END;
		else
			luaL_argerror(lua,3,"unknown seek type");
	}else
		origin = SEEK_SET;
	if(fseek(file,offset,origin))
		luaL_error(lua,"failed to seek file");
	return 0;
}
static int dsl_ReadFile(lua_State *lua){
	dsl_file *file;
	size_t bytes;
	char *buffer;
	
	file = getFile(lua,0);
	bytes = luaL_checknumber(lua,2);
	if(!bytes)
		luaL_argerror(lua,2,"cannot read zero bytes");
	buffer = lua_newuserdata(lua,bytes);
	if(readDslFile(file,buffer,bytes) != bytes)
		luaL_error(lua,"failed to read file");
	lua_pushlstring(lua,buffer,bytes);
	return 1;
}
static int dsl_WriteFile(lua_State *lua){
	dsl_file *file;
	size_t bytes;
	const char *buffer;
	
	file = getFile(lua,1);
	buffer = luaL_checklstring(lua,2,&bytes);
	if(getDslState(lua,1)->flags & DSL_CONNECTED_TO_SERVER)
		luaL_error(lua,"cannot write files after connecting to a server");
	if(bytes && writeDslFile(file,buffer,bytes) != bytes)
		luaL_error(lua,"failed to write file");
	return 0;
}

// TABLE I/O
static int dsl_SaveTable(lua_State *lua){
	const char *name;
	dsl_file *file;
	dsl_state *dsl;
	size_t bytes;
	void *data;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TTABLE);
	lua_settop(lua,2);
	name = lua_tostring(lua,1);
	data = packLuaTable(lua,&bytes);
	if(!data)
		luaL_error(lua,"%s",lua_tostring(lua,-1));
	dsl = getDslState(lua,1);
	if(dsl->flags & DSL_CONNECTED_TO_SERVER)
		luaL_error(lua,"cannot write files after connecting to a server");
	file = openDslFile(dsl,NULL,name,"wb",NULL);
	if(!file)
		luaL_argerror(lua,1,lua_pushfstring(lua,"%s: %s",getDslFileError(),name));
	if(writeDslFile(file,data,bytes) != bytes){
		free(data);
		closeDslFile(file);
		luaL_error(lua,"failed to write table to file");
	}
	free(data);
	closeDslFile(file);
	return 0;
}
static int dsl_LoadTable(lua_State *lua){
	const char *name;
	dsl_file *file;
	size_t bytes;
	void *data;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	name = lua_tostring(lua,1);
	file = openDslFile(getDslState(lua,1),NULL,name,"rb",&bytes);
	if(!file)
		luaL_argerror(lua,1,lua_pushfstring(lua,"%s: %s",getDslFileError(),name));
	data = malloc(bytes);
	if(!data){
		closeDslFile(file);
		luaL_error(lua,"failed to allocate memory");
	}
	if(readDslFile(file,data,bytes) != bytes){
		free(data);
		closeDslFile(file);
		luaL_error(lua,"failed to read table from file");
	}
	closeDslFile(file);
	if(!unpackLuaTable(lua,data,bytes)){
		free(data);
		luaL_error(lua,"%s",lua_tostring(lua,-1));
	}
	free(data);
	return 1;
}

// OPEN
int dslopen_file(lua_State *lua){
	// BASICS
	lua_register(lua,"CanWriteFiles",&CanWriteFiles);
	lua_register(lua,"GetScriptFilePath",&GetScriptFilePath);
	lua_register(lua,"GetPackageFilePath",&GetPackageFilePath);
	lua_register(lua,"IsSystemAccessAllowed",&IsSystemAccessAllowed);
	// STANDARD
	lua_register(lua,"dofile",&dsl_dofile);
	lua_register(lua,"loadfile",&dsl_loadfile);
	lua_register(lua,"require",&dsl_require);
	// RAW I/O
	lua_register(lua,"OpenFile",&dsl_OpenFile);
	lua_register(lua,"CloseFile",&dsl_CloseFile);
	lua_register(lua,"FlushFile",&dsl_FlushFile);
	lua_register(lua,"SeekFile",&dsl_SeekFile);
	lua_register(lua,"ReadFile",&dsl_ReadFile);
	lua_register(lua,"WriteFile",&dsl_WriteFile);
	// TABLE I/O
	lua_register(lua,"SaveTable",&dsl_SaveTable);
	lua_register(lua,"LoadTable",&dsl_LoadTable);
	return 0;
}