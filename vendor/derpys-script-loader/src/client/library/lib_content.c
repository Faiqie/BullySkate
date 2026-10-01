// DERPY'S SCRIPT LOADER: CONTENT LIBRARY

#include <dsl/dsl.h>

static int RebuildArchives(lua_State *lua){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	if(!getConfigBoolean(dsl->config,"allow_img_replacement"))
		return 0;
	makeArchiveWithContent(dsl,CONTENT_ACT_IMG,"Act/Act.img",DSL_CONTENT_PATH"Act.img");
	makeArchiveWithContent(dsl,CONTENT_CUTS_IMG,"Cuts/Cuts.img",DSL_CONTENT_PATH"Cuts.img");
	makeArchiveWithContent(dsl,CONTENT_TRIGGER_IMG,"DAT/Trigger.img",DSL_CONTENT_PATH"Trigger.img");
	makeArchiveWithContent(dsl,CONTENT_IDE_IMG,"Objects/ide.img",DSL_CONTENT_PATH"ide.img");
	makeArchiveWithContent(dsl,CONTENT_SCRIPTS_IMG,"Scripts/Scripts.img",DSL_CONTENT_PATH"Scripts.img");
	if(getConfigBoolean(dsl->config,"allow_world_replacement"))
		makeArchiveWithContent(dsl,CONTENT_WORLD_IMG,"Stream/World.img",DSL_CONTENT_PATH"World.img");
	return 0;
}
static int getContentIdentifier(const char *name){
	if(dslisenum(name,"ACT.IMG"))
		return CONTENT_ACT_IMG;
	if(dslisenum(name,"CUTS.IMG"))
		return CONTENT_CUTS_IMG;
	if(dslisenum(name,"TRIGGER.IMG"))
		return CONTENT_TRIGGER_IMG;
	if(dslisenum(name,"IDE.IMG"))
		return CONTENT_IDE_IMG;
	if(dslisenum(name,"SCRIPTS.IMG"))
		return CONTENT_SCRIPTS_IMG;
	if(dslisenum(name,"WORLD.IMG"))
		return CONTENT_WORLD_IMG;
	return -1;
}
static int RegisterGameFile(lua_State *lua){
	script_collection *collection;
	dsl_state *dsl;
	int id;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TSTRING);
	id = getContentIdentifier(lua_tostring(lua,1));
	if(id == -1)
		luaL_argerror(lua,1,"unknown or unsupported img archive");
	dsl = getDslState(lua,1);
	if(!dsl->content)
		luaL_error(lua,"too late to replace game files");
	collection = dsl->manager->running_collection;
	if(!collection)
		luaL_error(lua,"no DSL script running");
	if(!collection->lc || !addContentFile(dsl->content,collection->lc,id,lua_tostring(lua,2)))
		luaL_error(lua,"failed to replace game file");
	return 0;
}
static int GetContentHash(lua_State *lua){
	int id;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	id = getContentIdentifier(lua_tostring(lua,1));
	if(id == -1)
		luaL_argerror(lua,1,"unknown or unsupported img archive");
	lua_pushlightuserdata(lua,(void*)getContentHash(id));
	return 1;
}
int dslopen_content(lua_State *lua){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	if(dsl->flags & DSL_ADD_REBUILD_FUNC)
		lua_register(lua,"RebuildArchives",&RebuildArchives);
	if(dsl->flags & DSL_ADD_DEBUG_FUNCS)
		lua_register(lua,"GetContentHash",&GetContentHash);
	lua_register(lua,"RegisterGameFile",&RegisterGameFile); // void (string archive, string file)
	return 0;
}