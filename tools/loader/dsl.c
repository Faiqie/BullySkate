/* DERPY'S SCRIPT LOADER: CORE
	
	this is where most other systems are controlled from
	dsl.c itself is controlled directly by main.c / sv_main.c
	
*/

#include <dsl/dsl.h>
#include <dsl/library.h>
#include <string.h>
#include <time.h>
#include <lualib.h>

//#define COUNT_GLOBAL_FUNCTIONS

// Include patch stuff.
#ifndef DSL_SERVER_VERSION
#include <dsl/client/patch.h>
//#define DEBUG_UPDATES
#endif

// Define config versions.
#ifdef DSL_SERVER_VERSION
#define CONFIG_VERSION 3
#else
#define CONFIG_VERSION 3
#endif

// Hacky lua function declaration.
LUA_API const char* lua_getupvalue_unsafe(lua_State*,int,int);

// Globals for testing.
#ifdef DEBUG_UPDATES
static int updated_system = 0;
static int updated_keyboard = 0;
static int updated_scripts = 0;
#endif

// Lua panic.
static int atLuaPanic(lua_State *lua){
	#ifdef DSL_SERVER_VERSION
	printConsoleRaw(g_dsl ? g_dsl->console : NULL,CONSOLE_ERROR,"A severe Lua error has occurred.");
	#ifdef DSL_SYSTEM_PAUSE
	system(DSL_SYSTEM_PAUSE);
	#endif
	#else
	MessageBox(NULL,"A severe Lua error has occurred.",NULL,MB_OK);
	#endif
	return 0;
}

// Default config.
#ifdef DSL_CONFIG_FILE
#ifdef DSL_SERVER_VERSION
static int makeDefaultConfig(){
	FILE *file;
	
	file = fopen(DSL_CONFIG_FILE,"wb");
	if(!file)
		return 0;
	fprintf(file,"# derpy's script server: config\r\n\
config_version %d\r\n\
\r\n\
[SERVER_CONFIG]\r\n\
server_name Unconfigured Server\r\n\
server_info Come play!\r\n\
#server_icon icon.png              # 1:1 png for icon\r\n\
server_players 16                  # maximum amount of players\r\n\
server_ip 0.0.0.0                  # listening address (usually not to be changed)\r\n\
server_port 17017                  # listening port number (client defaults to 17017)\r\n\
server_hz 30                       # how often the server should update\r\n\
\r\n\
[SCRIPT_LOADING]\r\n\
dont_auto_start false          # prefer not to start script collections automatically\r\n\
force_auto_start_pref false    # ignore auto_start overrides set by script collections\r\n\
\r\n\
[CONSOLE_PREFERENCES]\r\n\
console_logging true      # log console messages to a file\r\n\
console_warnings false    # show warnings about server performance\r\n\
console_no_input false    # disable console input\r\n\
\r\n\
[BLACKLIST_AND_WHITELIST]\r\n\
whitelist_instead false    # only allow whitelisted ips instead of stopping blacklisted ones\r\n\
#blacklist_ip 0.0.0.0      # add as many blacklist_ip or whitelist_ip lines as you want\r\n\
#whitelist_ip 0.0.0.0\r\n\
\r\n\
[UNRESTRICTED_SYSTEM_ACCESS]\r\n\
allow_system_access false                 # fully load io / os / loadlib libraries (dangerous with downloaded / untrusted scripts)\r\n\
yes_im_sure_i_know_what_im_doing false    # seriously this gives all scripts you run full access to your system",CONFIG_VERSION);
	fclose(file);
	return 1;
}
#else
static int makeDefaultConfig(){
	FILE *file;
	
	if(!checkDslPathExists(DSL_CONFIG_FILE,1))
		return 0;
	file = fopen(DSL_CONFIG_FILE,"wb");
	if(!file)
		return 0;
	fprintf(file,"# derpy's script loader: config\r\n\
config_version %d\r\n\
\r\n\
[NETWORKING]\r\n\
username player           # your username for servers\r\n\
allow_networking false    # allow connection to servers\r\n\
\r\n\
[SERVER_LIST]\r\n\
list_server localhost:17017    # allow listing to be shown for this server (add as many list_server lines as you want)\r\n\
\r\n\
[SCRIPT_LOADING]\r\n\
dont_auto_start false          # prefer not to start script collections automatically\r\n\
force_auto_start_pref false    # ignore auto_start overrides set by script collections\r\n\
\r\n\
[CONSOLE_PREFERENCES]\r\n\
console_key 0x29                                   # directinput keyboard scan code\r\n\
console_font Lucida Console                        # must be a font installed on the system\r\n\
console_scale 1.0                                  # scale for the text in the console window\r\n\
console_color BB000000                             # background color specified in AARRGGBB format\r\n\
#console_image _derpy_script_loader/example.png    # 1:1 png to replace the default snake signature\r\n\
console_logging true                               # log console messages during each stage to a file\r\n\
\r\n\
[IMG_FILE_REPLACEMENT]\r\n\
allow_img_replacement true      # create temporary copies of img archives that include files supplied by script collections\r\n\
allow_world_replacement true    # world.img is very large and can drastically increase load times on slow storage\r\n\
\r\n\
[UNRESTRICTED_SYSTEM_ACCESS]\r\n\
allow_system_access false                 # fully load io / os / loadlib libraries (dangerous with downloaded / untrusted scripts)\r\n\
yes_im_sure_i_know_what_im_doing false    # seriously this gives all scripts you run full access to your system",CONFIG_VERSION);
	fclose(file);
	return 1;
}
#endif
#endif

// Basic commands.
static void clearConsole(script_console *console,int argc,char **argv){
	clearScriptConsole(console);
}
static void disallowConnect(script_console *console,int argc,char **argv){
	printConsoleRaw(console,CONSOLE_ERROR,"Networking is not enabled, please enable it in your config if you'd like to play online.");
}

// Init components.
#ifdef DSL_SERVER_VERSION
#define showInitFail(msg) printConsoleError(NULL,"\r%s\n",msg);
#else
#define showInitFail(msg) MessageBox(NULL,msg,NULL,MB_OK)
#endif
static int initConfig(dsl_state *state,int *generated){
	int status;
	
	*generated = 0;
	#ifdef DSL_CONFIG_FILE
	state->config = loadConfigSettings(DSL_CONFIG_FILE,&status);
	if(state->config){
		if(getConfigInteger(state->config,"config_version") != CONFIG_VERSION){
			if(!makeDefaultConfig()){
				showInitFail("failed to update config file");
				return 1;
			}
			state->config = loadConfigSettings(DSL_CONFIG_FILE,&status);
			*generated = 2;
		}
	}else if(status == CONFIG_MISSING){
		if(!makeDefaultConfig()){
			showInitFail("failed to generate default config");
			return 1;
		}
		state->config = loadConfigSettings(DSL_CONFIG_FILE,&status);
		*generated = 1;
	}
	if(!state->config && status != CONFIG_EMPTY){
		showInitFail("failed to load config");
		return 1;
	}
	#ifdef DSL_SERVER_VERSION
	if(*generated){
		printConsoleRaw(NULL,CONSOLE_OUTPUT,"A new config file has been generated, please configure your server before restarting.\n");
		return 1;
	}
	#endif
	#endif
	if(getConfigBoolean(state->config,"allow_system_access") && getConfigBoolean(state->config,"yes_im_sure_i_know_what_im_doing"))
	#ifdef DSL_DISABLE_SYSTEM_ACCESS
		state->flags |= DSL_WARN_SYSTEM_S;
	#else
		state->flags |= DSL_SYSTEM_ACCESS;
	#endif
	#ifndef DSL_SERVER_VERSION
	if(getConfigBoolean(state->config,"allow_networking")){
		if(state->flags & DSL_SYSTEM_ACCESS){
			state->flags |= DSL_WARN_SYSTEM_N;
			state->flags &= ~DSL_SYSTEM_ACCESS; // prevent ass fucking of user's pc
		}
		state->flags |= DSL_SERVER_ACCESS;
	}
	if(getConfigBoolean(state->config,"dev_add_rebuild_function"))
		state->flags |= DSL_ADD_REBUILD_FUNC;
	state->console_key = getConfigValue(state->config,"console_key") ? getConfigInteger(state->config,"console_key") : DIK_GRAVE;
	#endif
	if(getConfigBoolean(state->config,"dev_add_debug_functions"))
		state->flags |= DSL_ADD_DEBUG_FUNCS;
	return 0;
}
#ifndef DSL_SERVER_VERSION
static int initRender(dsl_state *state,void *device){
	if(!device)
		return 0;
	if(state->render = createRender(device)){
		state->dwrite_cycle = 1;
		startRenderDwriteCycle(state->render);
		return 0;
	}
	showInitFail("failed to setup renderer");
	return 1;
}
static int initContent(dsl_state *state){
	if(state->render && ~state->flags & DSL_ADD_REBUILD_FUNC)
		return 0;
	if(state->content = createContentManager(state))
		return 0;
	showInitFail("failed to setup content replacer");
	return 1;
}
#endif
static int initEvents(dsl_state *state){
	if(state->events = createScriptEvents())
		return 0;
	showInitFail("failed to setup event handler");
	return 1;
}
static int initConsole(dsl_state *state){
	#ifdef DSL_SERVER_VERSION
	char logname[256];
	char *logsuffix;
	time_t timer;
	int count;
	#else
	const char *logname;
	char buffer[MAX_PATH];
	const char *image;
	const char *sizes;
	const char *argbs;
	float size;
	DWORD argb;
	#endif
	
	#ifdef DSL_SERVER_VERSION
	state->console = createScriptConsole();
	#else
	if(sizes = getConfigValue(state->config,"console_scale")){
		size = strtod(sizes,NULL);
		if(size < 0.1f)
			size = 0.1f;
		else if(size > 4.0f)
			size = 4.0f;
	}else
		size = 1.0f;
	if(argbs = getConfigValue(state->config,"console_color"))
		argb = strtoul(argbs,NULL,16);
	else
		argb = 0xBB000000;
	#ifdef DSL_SIGNATURE_PNG
	if((image = getConfigString(state->config,"console_image")) && strlen(image) < MAX_PATH){
		strcpy(buffer,image);
		state->console = createScriptConsole(state->render,buffer,getConfigString(state->config,"console_font"),size,1,argb);
	}else
		state->console = createScriptConsole(state->render,DSL_SIGNATURE_PNG,getConfigString(state->config,"console_font"),size,0,argb);
	#else
	state->console = createScriptConsole(state->render,NULL,getConfigString(state->config,"console_font"),size,0);
	#endif
	#endif
	if(!state->console){
		showInitFail("failed to setup console");
		return 1;
	}
	#ifdef DSL_LOGGING_PATH
	if(getConfigBoolean(state->config,"console_logging")){
		#ifdef DSL_SERVER_VERSION
		time(&timer);
		logsuffix = logname + strftime(logname,sizeof(logname),DSL_LOGGING_PATH"%Y_%m_%d",localtime(&timer));
		strcpy(logsuffix,".log");
		#ifdef _WIN32
		for(count = 2;GetFileAttributes(logname) != INVALID_FILE_ATTRIBUTES;count++)
		#else
		for(count = 2;!access(logname,F_OK);count++)
		#endif
			sprintf(logsuffix,"_%d.log",count);
		#else
		if(!state->render)
			logname = DSL_LOGGING_PATH"pre_init.log";
		else if(!state->game)
			logname = DSL_LOGGING_PATH"init.log";
		else
			logname = DSL_LOGGING_PATH"main.log";
		#endif
		if(!checkDslPathExists(DSL_LOGGING_PATH,0) || !setScriptConsoleLogging(state->console,logname)){
			showInitFail("failed to setup console log");
			return 1;
		}
	}
	#endif
	#ifdef DSL_SIGNATURE_TEXT
	printConsoleRaw(state->console,CONSOLE_OUTPUT,DSL_SIGNATURE_TEXT);
	if(state->flags & DSL_ADD_DEBUG_FUNCS)
		printConsoleRaw(state->console,CONSOLE_OUTPUT," + debug functions");
	if(state->flags & DSL_ADD_REBUILD_FUNC)
		printConsoleRaw(state->console,CONSOLE_OUTPUT," + reload function");
	#ifdef DSL_SERVER_VERSION
	if(getConfigBoolean(state->config,"console_warnings"))
		state->flags |= DSL_SHOW_TICK_WARNINGS;
	#ifdef _WIN32
	printf("\r \n> "); // extra line after signature
	#else
	printf("\n");
	#endif
	fflush(stdout);
	#endif
	#endif
	return 0;
}
static int initCmdlist(dsl_state *state){
	if(state->cmdlist = createScriptCommandList(state)){
		setScriptCommandEx(state->cmdlist,"clear",TEXT_HELP_CLEAR,&clearConsole,state->console,1);
		return 0;
	}
	showInitFail("failed to setup command parser");
	return 1;
}
static int initNetwork(dsl_state *state){
	#ifndef DSL_SERVER_VERSION
	if(!state->render)
		return 0;
	if(~state->flags & DSL_SERVER_ACCESS){
		setScriptCommandEx(state->cmdlist,"connect",TEXT_HELP_CONNECT,&disallowConnect,state->console,1);
		return 0;
	}
	#endif
	if(state->network = setupNetworking(state))
		return 0;
	showInitFail("failed to setup networking");
	return 1;
}
static int initManager(dsl_state *state){
	if(state->manager = createScriptManager(state))
		return 0;
	showInitFail("failed to setup script manager");
	return 1;
}
static int initLoader(dsl_state *state){
	if(state->loader = createScriptLoader(state)){
		startScriptLoader(state,state->loader);
		return 0;
	}
	showInitFail("failed to start scripts");
	return 1;
}

// Resize pools.
#ifndef DSL_SERVER_VERSION
void resizePools(config_file *cfg){
	unsigned scale;
	unsigned size;
	
	scale = getConfigInteger(cfg,"resize_pool_scale");
	if(!scale)
		return;
	if(size = getConfigInteger(cfg,"resize_pool_ptr_node") * scale)
		replaceImmediateValue((void*)0x44D34F,(void*)size); // 15000 GetPtrNodePool
	if(size = getConfigInteger(cfg,"resize_pool_entry_info_node") * scale)
		replaceImmediateValue((void*)0x44D38D,(void*)size); // 2000  GetEntryInfoNodePool
	if(size = getConfigInteger(cfg,"resize_pool_vehicle") * scale)
		replaceImmediateValue((void*)0x44D3CB,(void*)size); // 15    GetVehiclePool
	if(size = getConfigInteger(cfg,"resize_pool_ped") * scale)
		replaceImmediateValue((void*)0x44D3F6,(void*)size); // 24    GetPedPool, 8 others
	if(size = getConfigInteger(cfg,"resize_pool_joint_constraint") * scale)
		replaceImmediateValue((void*)0x44D5CB,(void*)size); // 48    GetJointConstraintPool, 3 others
	if(size = getConfigInteger(cfg,"resize_pool_weapon") * scale)
		replaceImmediateValue((void*)0x44D693,(void*)size); // 57    weapons
	if(size = getConfigInteger(cfg,"resize_pool_lua_script") * scale)
		replaceImmediateValue((void*)0x44D6D1,(void*)size); // 8     GetLuaScriptPool
	if(size = getConfigInteger(cfg,"resize_pool_dummy") * scale)
		replaceImmediateValue((void*)0x44D70F,(void*)size); // 300   GetDummyPool
	if(size = getConfigInteger(cfg,"resize_pool_prop_anim") * scale)
		replaceImmediateValue((void*)0x44D74D,(void*)size); // 220   GetPropAnimPool
	if(size = getConfigInteger(cfg,"resize_pool_building") * scale)
		replaceImmediateValue((void*)0x44D78B,(void*)size); // 2250  GetBuildingPool
	if(size = getConfigInteger(cfg,"resize_pool_treadable") * scale)
		replaceImmediateValue((void*)0x44D7C9,(void*)size); // 1     GetTreadablePool
	if(size = getConfigInteger(cfg,"resize_pool_col_model") * scale)
		replaceImmediateValue((void*)0x44D841,(void*)size); // 4150  GetColModelPool
	if(size = getConfigInteger(cfg,"resize_pool_stimulus") * scale)
		replaceImmediateValue((void*)0x44D87F,(void*)size); // 87    GetStimulusPool
	if(size = getConfigInteger(cfg,"resize_pool_object") * scale)
		replaceImmediateValue((void*)0x44D8BD,(void*)size); // 275   GetObjectPool
	if(size = getConfigInteger(cfg,"resize_pool_projectile") * scale)
		replaceImmediateValue((void*)0x44D8FB,(void*)size); // 35    GetProjectilePool
	if(size = getConfigInteger(cfg,"resize_pool_cut_scene") * scale)
		replaceImmediateValue((void*)0x44D939,(void*)size); // 30    GetCutScenePool
	if(size = getConfigInteger(cfg,"resize_pool_unknown") * scale)
		replaceImmediateValue((void*)0x44D9B3,(void*)size); // 200   ?
}
#endif

// Patch peds.
#ifndef DSL_SERVER_VERSION
static void* __cdecl getPedByHandle(int handle,int mode){
	game_pool *peds;
	unsigned index;
	void *ped;
	
	if(handle == -1)
		return NULL;
	if(mode == 3)
		return *(void**)0xC1AEA8;
	index = handle & 0xFF;
	peds = getGamePedPool();
	if(index >= peds->limit || peds->flags[index] & GAME_POOL_INVALID)
		return NULL;
	ped = peds->array + peds->size * index;
	if(getGamePedId(ped) == handle)
		return ped;
	return NULL;
}
static void applyPedPoolPatch(){
	replaceCodeWithJump((void*)0x5C7380,&getPedByHandle);
	replaceCodeWithJump((void*)0x4A7E80,(void*)0x4A7FBD); // disable accessory manager update
	// 0x4788EA 0x5FB4C9 0x6666E0 0x666909 0x66CA0F 0x758930 0x8228A0 0x8273C3 0x82745E 0x827571 0x82777A hardcoded 24?
}
#endif

// Init globals.
#ifdef COUNT_GLOBAL_FUNCTIONS
static unsigned countScriptFunctions(lua_State *lua){
	unsigned count;
	
	count = 0;
	lua_pushnil(lua);
	while(lua_next(lua,LUA_GLOBALSINDEX)){
		if(lua_isfunction(lua,-1))
			count++;
		lua_pop(lua,1);
	}
	return count;
}
#endif
static void initScriptGlobals(dsl_state *state){
	lua_State *lua;
	unsigned count;
	int stack;
	
	lua = state->lua;
	lua_pushstring(lua,DSL_REGISTRY_KEY);
	lua_pushlightuserdata(lua,state);
	lua_rawset(lua,LUA_REGISTRYINDEX); // registry[DSL_REGISTRY_KEY] = state
	lua_pushstring(lua,"gDerpyScriptLoader");
	lua_pushnumber(lua,DSL_VERSION_LUA);
	lua_settable(lua,LUA_GLOBALSINDEX);
	/*lua_pushstring(lua,"LUA_PATH");
	lua_pushstring(lua,DSL_PACKAGE_PATH"?.lua");
	lua_settable(lua,LUA_GLOBALSINDEX);*/
	stack = lua_gettop(lua);
	luaopen_base(lua);
	lua_settop(lua,stack);
	luaopen_table(lua);
	lua_settop(lua,stack);
	luaopen_string(lua);
	lua_settop(lua,stack);
	luaopen_math(lua);
	lua_settop(lua,stack);
	luaopen_debug(lua);
	lua_settop(lua,stack);
	#ifndef DSL_DISABLE_SYSTEM_ACCESS
	if(state->flags & DSL_SYSTEM_ACCESS){
		luaopen_io(lua);
		lua_settop(lua,stack);
		luaopen_loadlib(lua);
		lua_settop(lua,stack);
	}
	#endif
	#ifdef COUNT_GLOBAL_FUNCTIONS
	count = countScriptFunctions(lua);
	#endif
	loadScriptLibraries(lua);
    extern int dslopen_fakie(lua_State*); dslopen_fakie(lua);
	#ifdef COUNT_GLOBAL_FUNCTIONS
	printConsoleOutput(state->console,"loadScriptLibraries functions: %d",countScriptFunctions(lua)-count);
	#endif
}

// Seed random.
static void seedRandom(lua_State *lua){
	lua_pushstring(lua,"math");
	lua_gettable(lua,LUA_GLOBALSINDEX);
	if(lua_istable(lua,-1)){
		lua_pushstring(lua,"randomseed");
		lua_gettable(lua,-2);
		if(lua_isfunction(lua,-1)){
			#ifdef _WIN32
			lua_pushnumber(lua,GetTickCount());
			#else
			lua_pushnumber(lua,time(NULL));
			#endif
			lua_call(lua,1,0);
		}else
			lua_pop(lua,1); // pop random
	}
	lua_pop(lua,1); // pop math
}

// DSL persistence.
#ifndef DSL_SERVER_VERSION
static void loadDslData(dsl_state *state,size_t *dsldata){
	lua_State *lua;
	
	lua = state->lua;
	if(!unpackLuaTable(lua,dsldata+1,*dsldata)){
		printConsoleRaw(state->console,CONSOLE_ERROR,lua_tostring(lua,-1));
		lua_pop(lua,1);
		free(dsldata);
		return;
	}
	state->dsl_data = luaL_ref(lua,LUA_REGISTRYINDEX);
	free(dsldata);
}
size_t* saveDslData(dsl_state *state){
	lua_State *lua;
	size_t *dsldata;
	size_t bytes;
	void *data;
	
	dsldata = NULL;
	if(state->dsl_data != LUA_NOREF){
		lua = state->lua;
		lua_rawgeti(lua,LUA_REGISTRYINDEX,state->dsl_data);
		if(lua_istable(lua,-1)){
			//stripLuaTable(lua);
			if(data = packLuaTable(lua,&bytes)){
				if(dsldata = malloc(sizeof(size_t) + bytes)){
					*dsldata = bytes;
					memcpy(dsldata+1,data,bytes);
				}
				free(data);
			}else{
				lua_pushstring(lua,"failed to save persistent data: ");
				lua_insert(lua,-2);
				lua_concat(lua,2);
				printConsoleRaw(state->console,CONSOLE_ERROR,lua_tostring(lua,-1));
				lua_pop(lua,1);
			}
		}else
			lua_pop(lua,1);
	}
	return dsldata;
}
#endif

// DSL initialization.
void closeDsl(dsl_state *state){
	size_t *dsldata;
	
	#ifndef DSL_SERVER_VERSION
	if(!state->game)
	#else
	if(state->events)
		runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"ServerShutdown",0);
	#endif
		lua_close(state->lua);
	if(state->loader)
		destroyScriptLoader(state->loader);
	if(state->manager)
		destroyScriptManager(state->manager);
	if(state->network)
		cleanupNetworking(state,state->network);
	if(state->cmdlist)
		destroyScriptCommandList(state->cmdlist);
	if(state->console)
		destroyScriptConsole(state->console);
	if(state->events)
		destroyScriptEvents(state->events);
	#ifndef DSL_SERVER_VERSION
	if(state->content)
		destroyContentManager(state->content);
	if(state->render)
		destroyRender(state->render);
	#endif
	freeConfigSettings(state->config);
	free(state);
}
dsl_state* openDsl(void *game,void *device,size_t *dsldata){
	dsl_state *state;
	int generatedcfg;
	
	state = calloc(1,sizeof(dsl_state));
	if(!state){
		showInitFail("failed to allocate dsl state");
		if(dsldata)
			free(dsldata);
		return NULL;
	}
	#ifndef DSL_SERVER_VERSION
	if(state->game = game){
		state->lua = getGameLuaState(game);
		lua_settop(state->lua,0); // pop leftovers from the game loading standard libraries (because it normally leaves them)
	}else
	#endif
		state->lua = lua_open();
	lua_atpanic(state->lua,&atLuaPanic);
	#ifndef DSL_SERVER_VERSION
	state->last_frame = getGameTimer(); // server handles timing in sv_main.c
	state->save_data = LUA_NOREF;
	#endif
	state->dsl_data = LUA_NOREF;
	if(initConfig(state,&generatedcfg)
	#ifndef DSL_SERVER_VERSION
	|| initRender(state,device) || initContent(state)
	#endif
	|| initEvents(state) || initConsole(state) || initCmdlist(state) || initNetwork(state)){
		if(dsldata)
			free(dsldata);
		closeDsl(state);
		return NULL;
	}
	#ifndef DSL_SERVER_VERSION
	if(!device){
		resizePools(state->config);
		//if(getConfigBoolean(state->config,"try_experimental_ped_pool_patch"))
			//applyPedPoolPatch();
	}
	#endif
	initScriptGlobals(state);
	#ifdef DSL_SERVER_VERSION
	seedRandom(state->lua);
	#else
	if(dsldata)
		loadDslData(state,dsldata);
	#endif
	if(initManager(state) || initLoader(state)){
		closeDsl(state);
		return NULL;
	}
	updateScriptManagerInit(state->manager,state->lua);
	if(generatedcfg == 1)
		printConsoleRaw(state->console,CONSOLE_OUTPUT,"Generated default config file.");
	else if(generatedcfg == 2)
		printConsoleRaw(state->console,CONSOLE_OUTPUT,"Updated config file to new version.");
	if(state->flags & DSL_WARN_SYSTEM_S)
		printConsoleRaw(state->console,CONSOLE_WARNING,"System access is not supported in \"S\" versions.");
	else if(state->flags & DSL_WARN_SYSTEM_N)
		printConsoleRaw(state->console,CONSOLE_WARNING,"System access is not allowed when networking is enabled.");
	return state;
}

// Init content.
#ifndef DSL_SERVER_VERSION
static void* __cdecl openIdeImg(const char *name,const char *mode){
	return (*(void*(__cdecl*)(const char*,const char*))0x42D260)(DSL_CONTENT_PATH"ide.img",mode);
}
static void* __cdecl openIdeDir(const char *name,const char *mode){
	return (*(void*(__cdecl*)(const char*,const char*))0x42D260)(DSL_CONTENT_PATH"ide.dir",mode);
}
static void* __cdecl openWorldImg(const char *name,const char *mode){
	return (*(void*(__cdecl*)(const char*,const char*))0x42D260)(DSL_CONTENT_PATH"World.img",mode);
}
void initDslContent(dsl_state *state){
	if(!generateContentHashes(state))
		printConsoleWarning(state->console,"failed to hash game files");
	if(!getConfigBoolean(state->config,"allow_img_replacement"))
		return;
	if(!checkDslPathExists(DSL_CONTENT_PATH,0)){
		printConsoleError(state->console,"failed to create img cache: %s",DSL_CONTENT_PATH);
		return;
	}
	if(makeArchiveWithContent(state,CONTENT_ACT_IMG,"Act/Act.img",DSL_CONTENT_PATH"Act.img")){
		replaceImmediateValue((void*)0x5F6F9D,DSL_CONTENT_PATH"Act.img");
		replaceImmediateValue((void*)0x5FB609,DSL_CONTENT_PATH"Act.img");
		replaceImmediateValue((void*)0x5F58F5,DSL_CONTENT_PATH"Act.dir");
		replaceImmediateValue((void*)0x5F590F,DSL_CONTENT_PATH"Act.dir");
	}
	if(makeArchiveWithContent(state,CONTENT_CUTS_IMG,"Cuts/Cuts.img",DSL_CONTENT_PATH"Cuts.img")){
		replaceImmediateValue((void*)0x6C46F0,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C513A,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C57A4,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C5A49,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C5CB8,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C6047,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C617C,DSL_CONTENT_PATH"Cuts.img");
		replaceImmediateValue((void*)0x6C378F,DSL_CONTENT_PATH"Cuts.dir");
	}
	if(makeArchiveWithContent(state,CONTENT_TRIGGER_IMG,"DAT/Trigger.img",DSL_CONTENT_PATH"Trigger.img")){
		replaceImmediateValue((void*)0x6D3D59,DSL_CONTENT_PATH"Trigger.img");
		replaceImmediateValue((void*)0x6D3EE0,DSL_CONTENT_PATH"Trigger.dir");
		replaceImmediateValue((void*)0x6D3EF6,DSL_CONTENT_PATH"Trigger.dir");
	}
	if(makeArchiveWithContent(state,CONTENT_IDE_IMG,"Objects/ide.img",DSL_CONTENT_PATH"ide.img")){
		replaceFunctionCall((void*)0x42CD3A,5,&openIdeImg);
		replaceFunctionCall((void*)0x42CCAA,5,&openIdeDir);
	}
	if(makeArchiveWithContent(state,CONTENT_SCRIPTS_IMG,"Scripts/Scripts.img",DSL_CONTENT_PATH"Scripts.img")){
		replaceImmediateValue((void*)0x5D8CFB,DSL_CONTENT_PATH"Scripts.img");
		replaceImmediateValue((void*)0x5DC33F,DSL_CONTENT_PATH"Scripts.dir");
		replaceImmediateValue((void*)0x5DC35F,DSL_CONTENT_PATH"Scripts.dir");
	}
	if(getConfigBoolean(state->config,"allow_world_replacement") && makeArchiveWithContent(state,CONTENT_WORLD_IMG,"Stream/World.img",DSL_CONTENT_PATH"World.img")){
		replaceImmediateValue((void*)0x42F2AD,DSL_CONTENT_PATH"World.img");
		replaceImmediateValue((void*)0x73AC32,DSL_CONTENT_PATH"World.img");
		replaceFunctionCall((void*)0x52E124,5,&openWorldImg); // open World.img
		replaceCodeWithJump((void*)0x52E087,(void*)0x52E09A); // skip name copying
	}
	freeContentListings(state->content);
}
#endif

// Input updates.
#ifndef DSL_SERVER_VERSION
static void processKeyboardPress(dsl_state *state,int key){
	if(key == state->console_key){
		if(state->render && (state->show_console = !state->show_console))
			refocusScriptConsole(state->console);
	}else if(state->show_console && sendScriptConsoleKeystroke(state->console,key) && !processScriptCommandLine(state->cmdlist,getScriptConsoleText(state->console)))
		printConsoleRaw(state->console,CONSOLE_ERROR,"command does not exist");
	runScriptEvent(state->events,state->lua,EVENT_KEY_DOWN_UPDATE,(void*)key);
}
void updateDslKeyboard(dsl_state *state,char *keyboard){
	char *kb;
	int key;
	
	kb = state->keyboard;
	for(key = 0;key < 0x100;key++)
		if(keyboard[key] & 0x80)
			if(kb[key])
				kb[key] = DSL_KEY_PRESSED;
			else{
				kb[key] = DSL_KEY_JUST_PRESSED;
				processKeyboardPress(state,key);
			}
		else if(kb[key])
			kb[key] = kb[key] == DSL_KEY_JUST_RELEASED ? 0 : DSL_KEY_JUST_RELEASED;
		else
			kb[key] = 0;
	if(state->show_console || (state->network && shouldNetworkDisableKeys(state->network)))
		memset(keyboard,0,0x100);
	#ifdef DEBUG_UPDATES
	updated_keyboard++;
	#endif
}
void updateDslMouse(dsl_state *state,DIMOUSESTATE *mouse){
	memcpy(&state->mouse2,&state->mouse,sizeof(DIMOUSESTATE));
	memcpy(&state->mouse,mouse,sizeof(DIMOUSESTATE));
}
void updateDslController(dsl_state *state,char *controller){
	lua_State *lua;
	int id;
	
	lua = state->lua;
	id = *(int*)(controller+0x138);
	if(id == 0)
		id = getGamePrimaryControllerIndex();
	else if(id == 1)
		id = getGameSecondaryControllerIndex();
	lua_pushnumber(lua,id);
	runLuaScriptEvent(state->events,lua,LOCAL_EVENT,"ControllerUpdating",1);
	lua_pop(lua,1);
}
void updateDslControllers(dsl_state *state){
	runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"ControllersUpdated",0);
}
#endif

// Drawing utility.
#ifndef DSL_SERVER_VERSION
static void runExtraDrawCycle(dsl_state *state,int type){
	lua_State *lua;
	
	if(!state->dwrite_cycle){
		state->dwrite_cycle = 1;
		startRenderDwriteCycle(state->render);
	}
	if(startRender(state->render)){
		lua = state->lua;
		updateScriptManagerUpdate(state->manager,lua,type);
		runScriptEvent(state->events,lua,EVENT_MANAGER_UPDATE,(void*)type);
		finishRender(state->render);
	}
}
#endif

// Save data.
#ifndef DSL_SERVER_VERSION
void readDslSaveData(dsl_state *state,void *data,size_t bytes){
	lua_State *lua;
	
	lua = state->lua;
	if(state->save_data != LUA_NOREF)
		luaL_unref(lua,LUA_REGISTRYINDEX,state->save_data);
	if(!data || !bytes)
		lua_newtable(lua); // no save data (so make a fresh table)
	else if(!unpackLuaTable(lua,data,bytes)){
		lua_pushstring(lua,"failed to load extra save data: ");
		lua_insert(lua,-2);
		lua_concat(lua,2);
		printConsoleRaw(state->console,CONSOLE_ERROR,lua_tostring(lua,-1));
		lua_pop(lua,1); // pop error message
		lua_newtable(lua);
	}
	state->save_data = luaL_ref(lua,LUA_REGISTRYINDEX); // extra save data
}
int writeDslSaveData(dsl_state *state,HANDLE file){
	lua_State *lua;
	size_t bytes,write;
	void *data;
	
	lua = state->lua;
	lua_rawgeti(lua,LUA_REGISTRYINDEX,state->save_data);
	if(!lua_istable(lua,-1)){
		printConsoleRaw(state->console,CONSOLE_ERROR,"extra save data was corrupted");
		lua_pop(lua,1);
		return 0;
	}
	lua_pushnil(lua);
	if(!lua_next(lua,-2)){
		lua_pop(lua,1);
		return 1; // empty table so don't save anything
	}
	lua_pop(lua,2); // pop key / value we used to test if anything was in the table
	//stripLuaTable(lua);
	data = packLuaTable(lua,&bytes);
	if(!data){
		lua_pushstring(lua,"failed to save extra save data: ");
		lua_insert(lua,-2);
		lua_concat(lua,2);
		printConsoleRaw(state->console,CONSOLE_ERROR,lua_tostring(lua,-1));
		lua_pop(lua,1);
		return 0;
	}
	if(WriteFile(file,data,bytes,&write,NULL) && write == bytes){
		free(data);
		return 1;
	}
	free(data);
	printConsoleRaw(state->console,CONSOLE_ERROR,"failed to write extra save data");
	return 0;
}
#endif

// Main updates.
#ifdef DEBUG_UPDATES
static void drawTestUpdates(dsl_state *state){
	float size,ar,x,y;
	int key,kbstate;
	
	size = 0.02f;
	ar = getRenderAspect(state->render);
	for(key = 0;key < 0xFF;key++){
		x = ((key % 15) * size) / ar;
		y = 0.01f + (key / 15) * size;
		if(kbstate = state->keyboard[key+1])
			drawRenderRectangle(state->render,x,y,size/ar,size,kbstate == DSL_KEY_JUST_RELEASED ? 0xFFFF0000 : (kbstate == DSL_KEY_JUST_PRESSED ? 0xFF00FF00 : 0xFFFFFF00));
		else
			drawRenderRectangle(state->render,x,y,size/ar,size,key % 2 ? 0xFF323232 : 0xFF161616);
	}
	drawRenderRectangle(state->render,0.0f,0.0f,0.2f,0.01f,updated_system ? (updated_system == 1 ? 0xFF00FF00 : 0xFF0000FF) : 0xFFFF0000);
	drawRenderRectangle(state->render,0.2f,0.0f,0.2f,0.01f,updated_keyboard ? (updated_keyboard == 1 ? 0xFF00FF00 : 0xFF0000FF) : 0xFFFF0000);
	drawRenderRectangle(state->render,0.4f,0.0f,0.2f,0.01f,updated_scripts ? (updated_scripts == 1 ? 0xFF00FF00 : 0xFF0000FF) : 0xFFFF0000);
	updated_scripts = updated_keyboard = updated_system = 0;
}
#endif
#ifndef DSL_SERVER_VERSION
void updateDslBeforeSystem(dsl_state *state){
	lua_State *lua;
	unsigned timer;
	
	lua = state->lua;
	timer = getGameTimer();
	state->frame_time = timer - state->last_frame;
	state->last_frame = timer;
	if(state->render && !state->dwrite_cycle){
		state->dwrite_cycle = 1;
		startRenderDwriteCycle(state->render);
	}
	if(state->network)
		updateNetworking(state,state->network);
	updateScriptManagerUpdate(state->manager,lua,SYSTEM_THREAD);
	runScriptEvent(state->events,lua,EVENT_MANAGER_UPDATE,(void*)SYSTEM_THREAD);
	#ifdef DEBUG_UPDATES
	updated_system++;
	#endif
}
void updateDslAfterSkipped(dsl_state *state){
	if(state->dwrite_cycle){ // stop active dwrite cycle if the game loop is skipped (usually because the game is out of focus)
		state->dwrite_cycle = 0;
		finishRenderDwriteCycle(state->render);
	}
}
#endif
void updateDslAfterScripts(dsl_state *state){
	lua_State *lua;
	int thread;
	
	lua = state->lua;
	if(lua_gettop(lua)){
		lua_settop(lua,0);
		printConsoleRaw(state->console,CONSOLE_WARNING,"Lua stack was non-zero during main script update.");
	}
	#ifndef DSL_SERVER_VERSION
	if(state->render && !state->dwrite_cycle){
		state->dwrite_cycle = 1;
		startRenderDwriteCycle(state->render);
	}
	if(state->save_data == LUA_NOREF)
		readDslSaveData(state,NULL,0);
	#endif
	for(thread = PRE_GAME_THREAD;thread <= GAME_THREAD2;thread++){
		updateScriptManagerUpdate(state->manager,lua,thread);
		runScriptEvent(state->events,lua,EVENT_MANAGER_UPDATE,(void*)thread);
	}
	#ifdef DEBUG_UPDATES
	updated_scripts++;
	#endif
}
#ifndef DSL_SERVER_VERSION
void updateDslAfterWorld(dsl_state *state){
	if(state->render)
		runExtraDrawCycle(state,POST_WORLD_THREAD);
}
void updateDslBeforeFade(dsl_state *state){
	if(state->render)
		runExtraDrawCycle(state,PRE_FADE_THREAD);
}
void updateDslBeforeDraw(dsl_state *state){
	lua_State *lua;
	
	if(!state->render)
		return;
	if(!state->dwrite_cycle){
		state->dwrite_cycle = 1;
		startRenderDwriteCycle(state->render);
	}
	if(startRender(state->render)){
		lua = state->lua;
		updateScriptManagerUpdate(state->manager,lua,DRAWING_THREAD);
		runScriptEvent(state->events,lua,EVENT_MANAGER_UPDATE,(void*)DRAWING_THREAD);
		if(state->show_console && !drawScriptConsole(state->console) && ~state->flags & DSL_SHOWN_ERROR_WINDOW){
			MessageBox(NULL,getRenderErrorString(state->render),"Console error",MB_OK);
			state->flags |= DSL_SHOWN_ERROR_WINDOW;
		}
		if(state->network)
			updateNetworkPrompt(state,state->network);
		#ifdef DEBUG_UPDATES
		drawTestUpdates(state);
		#endif
		finishRender(state->render);
	}else if(getRenderError(state->render) != RENDER_FAIL_NOT_READY_FOR_DRAW && ~state->flags & DSL_SHOWN_ERROR_WINDOW){
		MessageBox(NULL,getRenderErrorString(state->render),"Render error",MB_OK);
		state->flags |= DSL_SHOWN_ERROR_WINDOW;
	}
	if(state->dwrite_cycle){
		state->dwrite_cycle = 0;
		finishRenderDwriteCycle(state->render);
	}
	if(state->network)
		updateNetworking2(state,state->network);
}
#endif

// Miscellaneous events.
#ifndef DSL_SERVER_VERSION
int canDslBuildPlayer(dsl_state *state,void *player,void *ped){
	lua_State *lua;
	int result;
	
	lua = state->lua;
	lua_pushboolean(lua,player == *(void**)0x20C5F68); // is player
	lua_pushboolean(lua,ped != *(void**)0xC1AEA8); // is cutscene (by checking if ped != player)
	result = runLuaScriptEvent(state->events,lua,LOCAL_EVENT,"PlayerBuilding",2);
	lua_pop(lua,2);
	return result;
}
void creatingDslPed(dsl_state *state){
	runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"PedBeingCreated",0);
}
int canDslSetPedStat(dsl_state *state,void *ped,int stat,int value){
	lua_State *lua;
	
	lua = state->lua;
	lua_pushnumber(lua,getGamePedId(ped));
	lua_pushnumber(lua,stat);
	lua_pushnumber(lua,value);
	value = runLuaScriptEvent(state->events,lua,LOCAL_EVENT,"PedStatOverriding",3);
	lua_pop(lua,3);
	return value;
}
void setDslCameraFade(dsl_state *state,int black,float timer){
	lua_State *lua;
	
	lua = state->lua;
	lua_pushnumber(lua,timer*1000);
	lua_pushboolean(lua,!black);
	runLuaScriptEvent(state->events,lua,LOCAL_EVENT,"CameraFaded",2);
	lua_pop(lua,2);
}
int canDslLoadSaveGame(dsl_state *state){
	return runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"SaveGameLoading",0);
}
void loadedDslGameScript(dsl_state *state,void *script,lua_State *lua,lua_State *idk){
	void *c,*s;
	int i;
	
	lua_pushstring(lua,"CreateNameSpace");
	lua_gettable(lua,LUA_GLOBALSINDEX);
	if(!lua_isfunction(lua,-1)){
		lua_pop(lua,1);
		return; // no CreateNameSpace? give up.
	}
	lua_getupvalue_unsafe(lua,-1,1);
	if(!lua_istable(lua,-1)){
		lua_pop(lua,2); // no script table? give up.
		return;
	}
	lua_pushstring(lua,(char*)script+4);
	lua_rawget(lua,-2);
	if(!lua_istable(lua,-1)){
		lua_pop(lua,3); // no script info table? give up.
		return;
	}
	lua_replace(lua,-3); // replace CreateNameSpace with the script info table
	lua_pop(lua,1); // pop the script table
	lua_pushstring(lua,"func_env");
	lua_rawget(lua,-2);
	if(!lua_istable(lua,-1)){
		lua_pop(lua,2); // no script environment? give up.
		return;
	}
	lua_replace(lua,-2); // replace script info table with script environment
	lua_pushstring(lua,(char*)script+4);
	lua_insert(lua,-2);
	runLuaScriptEvent(state->events,lua,LOCAL_EVENT,"NativeScriptLoaded",2); // (name, event)
	lua_pop(lua,2); // pop name and script environment
}
int canDslPauseGame(dsl_state *state){
	return runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"GameBeingPaused",0);
}
int canDslForceSleep(dsl_state *state){
	return runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"PlayerSleepCheck",0);
}
void setDslPedThrottle(dsl_state *state,void *ped){
	game_pool *pool;
	lua_State *lua;
	int index;
	
	pool = getGamePedPool();
	for(index = 0;index < pool->limit;index++)
		if(~pool->flags[index] & GAME_POOL_INVALID && pool->array + pool->size * index == ped)
			break;
	if(index == pool->limit)
		return;
	lua = state->lua;
	lua_pushnumber(lua,getGamePedId(ped));
	runLuaScriptEvent(state->events,lua,LOCAL_EVENT,"PedUpdateThrottle",1);
	lua_pop(lua,1);
}
void updateDslWindow(dsl_state *state,HWND window,int show){
	runDslWindowStyleEvent(state,window,show);
}
int canDslBeMinimized(dsl_state *state){
	return runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"WindowMinimizing",0);
}
int canDslGoFullscreen(dsl_state *state){
	return runLuaScriptEvent(state->events,state->lua,LOCAL_EVENT,"WindowGoingFullscreen",0);
}
#endif
