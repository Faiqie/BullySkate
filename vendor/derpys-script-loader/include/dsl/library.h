/* DERPY'S SCRIPT LOADER: LUA LIBRARIES
	
	all DSL lua functions are in the lualib source folder
	this file declares the initialization functions for all of them
	
*/

#ifndef DSL_LIBRARY_H
#define DSL_LIBRARY_H

#ifdef DSL_SERVER_VERSION
#define loadScriptLibraries(lua)\
	dslopen_command(lua);\
	dslopen_config(lua);\
	dslopen_console(lua);\
	dslopen_event(lua);\
	dslopen_file(lua);\
	dslopen_manager(lua);\
	dslopen_miscellaneous(lua);\
	dslopen_serialize(lua);\
	dslopen_server(lua)
#else
#define loadScriptLibraries(lua)\
	dslopen_camera(lua);\
	dslopen_client(lua);\
	dslopen_command(lua);\
	dslopen_config(lua);\
	dslopen_console(lua);\
	dslopen_content(lua);\
	dslopen_event(lua);\
	dslopen_file(lua);\
	dslopen_hook(lua);\
	dslopen_input(lua);\
	dslopen_locale(lua);\
	dslopen_manager(lua);\
	dslopen_miscellaneous(lua);\
	dslopen_ped(lua);\
	dslopen_pool(lua);\
	dslopen_render(lua);\
	dslopen_savedata(lua);\
	dslopen_serialize(lua);\
	dslopen_vehicle(lua)
#endif
int dslopen_camera(lua_State *lua);
int dslopen_client(lua_State *lua);
int dslopen_command(lua_State *lua);
int dslopen_config(lua_State *lua);
int dslopen_console(lua_State *lua);
int dslopen_content(lua_State *lua);
int dslopen_event(lua_State *lua);
int dslopen_file(lua_State *lua);
int dslopen_hook(lua_State *lua);
int dslopen_input(lua_State *lua);
int dslopen_locale(lua_State *lua);
int dslopen_manager(lua_State *lua);
int dslopen_miscellaneous(lua_State *lua);
int dslopen_ped(lua_State *lua);
int dslopen_pool(lua_State *lua);
int dslopen_render(lua_State *lua);
int dslopen_savedata(lua_State *lua);
int dslopen_serialize(lua_State *lua);
int dslopen_server(lua_State *lua);
int dslopen_vehicle(lua_State *lua);

#endif