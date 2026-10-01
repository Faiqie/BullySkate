// DERPY'S SCRIPT LOADER: COMMAND LIBRARY

#include <dsl/dsl.h>
#include <string.h>

#define REGISTRY_KEY DSL_REGISTRY_KEY "_command"

// TYPES
typedef struct command_script{
	// every script can have an array of commands associated with it
	struct command_script *next;
	struct command **commands;
	int allocated;
	int count;
	script *s; // can be NULL, any non DSL script is just treated equal and allowed to set / clear
}command_script;
typedef struct command{
	int ref;
	lua_State *lua; // only valid if ref isn't LUA_NOREF
	script *s;
}command; // name follows

// STATE
static int freeScripts(lua_State *lua){
	command_script *script,*next;
	command *cmd;
	dsl_state *dsl;
	int i;
	
	dsl = getDslState(lua,0);
	for(script = *(command_script**)lua_touserdata(lua,1);script;script = next){
		for(i = 0;i < script->count;i++){
			cmd = script->commands[i];
			if(dsl)
				setScriptCommand(dsl->cmdlist,(char*)(cmd+1),NULL,NULL);
			free(cmd);
		}
		if(script->commands)
			free(script->commands);
		next = script->next;
		free(script);
	}
	return 0;
}
static command_script** createScripts(lua_State *lua){
	command_script **scripts;
	
	lua_pushstring(lua,REGISTRY_KEY);
	scripts = lua_newuserdata(lua,sizeof(command_script*));
	*scripts = NULL;
	lua_newtable(lua);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeScripts);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lua_rawset(lua,LUA_REGISTRYINDEX);
	return scripts;
}
static command_script** getScripts(lua_State *lua){
	command_script **scripts;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	scripts = lua_touserdata(lua,-1);
	lua_pop(lua,1);
	return scripts;
}

// CLEANUP
static int destroyedScript(lua_State *lua,command_script **scripts,script *s){
	command_script *script,*prior;
	command *cmd;
	dsl_state *dsl;
	int i;
	
	script = *scripts;
	if(!script)
		return 0;
	if(script->s == s)
		*scripts = script->next;
	else
		for(prior = script;script = prior->next;prior = script)
			if(script->s == s){
				prior->next = script->next;
				break;
			}
	if(!script)
		return 0;
	dsl = getDslState(lua,0);
	for(i = 0;i < script->count;i++){
		cmd = script->commands[i];
		if(cmd->ref != LUA_NOREF)
			luaL_unref(lua,LUA_REGISTRYINDEX,cmd->ref);
		if(dsl)
			setScriptCommand(dsl->cmdlist,(char*)(cmd+1),NULL,NULL);
		free(cmd);
	}
	if(script->commands)
		free(script->commands);
	free(script);
	return 0;
}

// ADD
static command_script* getScript(command_script **scripts,script *s){
	command_script *script;
	
	for(script = *scripts;script;script = script->next)
		if(script->s == s)
			return script;
	if(script = calloc(1,sizeof(command_script))){
		script->next = *scripts;
		script->s = s;
		*scripts = script;
	}
	return script;
}
static command* addCommand(lua_State *lua,script *s,const char *name){
	command_script *script;
	command **commands;
	command *cmd;
	int resize;
	
	script = getScript(getScripts(lua),s);
	if(!script)
		return NULL;
	commands = script->commands;
	if(script->allocated == script->count){
		resize = script->allocated;
		if(!resize)
			resize = 1;
		commands = realloc(commands,sizeof(command*) * (resize *= 2)); // 2, 4, 8, 16, ...
		if(!commands)
			return NULL;
		script->commands = commands;
		script->allocated = resize;
	}
	cmd = malloc(sizeof(command) + strlen(name) + 1);
	if(!cmd)
		return NULL;
	cmd->s = s;
	cmd->ref = LUA_NOREF;
	strcpy((char*)(cmd+1),name);
	commands[script->count++] = cmd;
	return cmd;
}

// REMOVE
static int removeCommand(lua_State *lua,script *s,const char *name,script_cmdlist *cmdlist){
	command_script *script;
	command **commands;
	command *cmd;
	int i;
	
	for(script = *getScripts(lua);script;script = script->next)
		if(script->s == s){
			commands = script->commands;
			for(i = 0;i < script->count;i++){
				cmd = commands[i];
				if(areScriptCommandNamesEqual((char*)(cmd+1),name)){
					if(cmd->ref != LUA_NOREF)
						luaL_unref(lua,LUA_REGISTRYINDEX,cmd->ref);
					setScriptCommand(cmdlist,(char*)(cmd+1),NULL,NULL);
					free(cmd);
					for(i++;i < script->count;i++)
						commands[i-1] = commands[i];
					script->count--;
					return 1;
				}
			}
		}
	return 0;
}

// UTILITY
static script* getScriptContainingCommand(lua_State *lua,const char *name,int *none){
	command_script *script;
	int i;
	
	*none = 0;
	for(script = *getScripts(lua);script;script = script->next)
		for(i = 0;i < script->count;i++)
			if(areScriptCommandNamesEqual((char*)(script->commands[i]+1),name))
				return script->s;
	*none = 1;
	return NULL;
}
static void printCommandWarning(lua_State *lua,dsl_state *dsl,script *s,const char *name){
	script *already;
	int none;
	
	if(already = getScriptContainingCommand(lua,name,&none))
		printConsoleWarning(dsl->console,"%s\"%s\" cannot set \"/%s\" because \"%s\" already did",getDslPrintPrefix(dsl,0),getScriptName(s),name,getScriptName(already));
	else if(none)
		printConsoleWarning(dsl->console,"%s\"%s\" cannot set \"/%s\" because it is reserved by DSL",getDslPrintPrefix(dsl,0),getScriptName(s),name);
	else
		printConsoleWarning(dsl->console,"%s\"%s\" cannot set \"/%s\" because a native script already did",getDslPrintPrefix(dsl,0),getScriptName(s),name);
}
static void printCommandError(lua_State *lua,dsl_state *dsl){
	if(lua_isstring(lua,-1))
		printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),lua_tostring(lua,-1));
	else
		printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),TEXT_FAIL_UNKNOWN);
}

// COMMAND
static void scriptCommand(command *cmd,int argc,char **argv){
	script_block sb;
	dsl_state *dsl;
	lua_State *lua;
	int argi;
	
	if(cmd->ref == LUA_NOREF)
		return; // insanely unlikely that luaL_ref failed, but just a fail-safe
	lua = cmd->lua;
	dsl = getDslState(lua,0);
	if(!dsl || !lua_checkstack(lua,argc+LUA_MINSTACK))
		return;
	lua_rawgeti(lua,LUA_REGISTRYINDEX,cmd->ref);
	for(argi = 0;argi < argc;argi++)
		lua_pushstring(lua,argv[argi]);
	startScriptBlock(dsl->manager,cmd->s,&sb);
	if(lua_pcall(lua,argc,0,0)){
		printCommandError(lua,dsl);
		lua_pop(lua,1);
	}
	finishScriptBlock(dsl->manager,&sb,lua);
}

// LIBRARY
static int SetCommand(lua_State *lua){
	const char *name;
	dsl_state *dsl;
	script_cmdlist *cmdlist;
	script *s;
	command *cmd;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TFUNCTION);
	lua_settop(lua,4);
	name = lua_tostring(lua,1);
	dsl = getDslState(lua,1);
	cmdlist = dsl->cmdlist;
	s = dsl->manager->running_script;
	if(!*name)
		luaL_argerror(lua,1,"command name is empty");
	while(*name == '/' && name[1])
		name++;
	if(doesScriptCommandExist(cmdlist,name) && !removeCommand(lua,s,name,dsl->cmdlist)){
		if(s)
			printCommandWarning(lua,dsl,s,name);
		lua_pushboolean(lua,0);
		return 1;
	}
	cmd = addCommand(lua,s,name);
	if(!cmd || !setScriptCommandEx(cmdlist,name,lua_tostring(lua,4),&scriptCommand,cmd,lua_toboolean(lua,3)))
		luaL_error(lua,"failed to set command");
	lua_settop(lua,2);
	cmd->lua = dsl->lua;
	cmd->ref = luaL_ref(lua,LUA_REGISTRYINDEX);
	lua_pushboolean(lua,1);
	return 1;
}
static int RunCommand(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	if(lua_gettop(lua) > 1)
		luaL_argerror(lua,2,"only one argument allowed");
	lua_pushboolean(lua,processScriptCommandLine(getDslState(lua,1)->cmdlist,lua_tostring(lua,1)));
	return 1;
}
static int ClearCommand(lua_State *lua){
	const char *name;
	dsl_state *dsl;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	name = lua_tostring(lua,1);
	dsl = getDslState(lua,1);
	while(*name == '/' && name[1])
		name++;
	removeCommand(lua,dsl->manager->running_script,name,dsl->cmdlist);
	return 0;
}
static int DoesCommandExist(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	lua_pushboolean(lua,doesScriptCommandExist(getDslState(lua,1)->cmdlist,lua_tostring(lua,1)));
	return 1;
}
int dslopen_command(lua_State *lua){
	addScriptEventCallback(getDslState(lua,1)->events,EVENT_SCRIPT_CLEANUP,&destroyedScript,createScripts(lua));
	lua_register(lua,"SetCommand",&SetCommand); // boolean (string name, function callback, boolean raw, string help)
	lua_register(lua,"RunCommand",&RunCommand); // void (string line)
	lua_register(lua,"ClearCommand",&ClearCommand); // void (string name)
	lua_register(lua,"DoesCommandExist",&DoesCommandExist); // boolean (string name)
	return 0;
}