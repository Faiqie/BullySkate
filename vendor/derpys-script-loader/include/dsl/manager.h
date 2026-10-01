/* DERPY'S SCRIPT LOADER: SCRIPT MANAGER
	
	this is where lua scripts and threads are initialized and controlled
	the manager state is created by dsl.c and used for loading by loader.c
	threads belong to scripts that belong to script collections that belong to the script manager
	
*/

#ifndef DSL_MANAGER_H
#define DSL_MANAGER_H

#include "loader.h"

struct dsl_file;
struct dsl_state;

// SIZES
#define SCRIPT_BYTES 11020

// TYPES
typedef struct script_manager script_manager;
typedef struct script_collection script_collection;
typedef struct script script;
typedef struct thread thread;
typedef struct script_block{
	script_collection *c;
	script *s;
	thread *t;
	int b;
	void *so;
}script_block;

// COLLECTION FLAGS
#define SHUTDOWN_COLLECTION 1
#define EXPORT_COLLECTION 2

// SCRIPT FLAGS
#define SHUTDOWN_SCRIPT 1
#define INITIAL_SCRIPT_RUN 2
#define DISABLE_SCRIPT_FLOW 4
#define HASHED_SCRIPT 8

// THREAD TYPES
#define PRE_GAME_THREAD 0
#define GAME_THREAD 1
#define GAME_THREAD2 2
#define SYSTEM_THREAD 3
#define DRAWING_THREAD 4
#define POST_WORLD_THREAD 5
#define PRE_FADE_THREAD 6
#define TOTAL_THREAD_TYPES 7

// THREAD FLAGS
#define SHUTDOWN_THREAD 1
#define PASS_ARGS_TO_THREAD 2
#define START_MAIN_AFTER_THREAD 4
#define QUIT_SCRIPT_AFTER_THREAD 8
#define ERROR_RUNNING_THREAD 16
#define CLEANUP_THREAD 32
#define NAMED_THREAD 64

#ifdef __cplusplus
extern "C" {
#endif

// EXECUTION BLOCKS
void startScriptBlock(script_manager *sm,script *s,script_block *backup);
int finishScriptBlock(script_manager *sm,script_block *backup,lua_State *lua);

// THREAD UTILITY
thread* createThread(script *s,lua_State *lua,int nargs,int type,int flags,const char *name); // 1 function & nargs are popped, if successful a thread is pushed (otherwise nil is pushed)
void destroyThread(thread *t,lua_State *lua,int cleanup);
int shutdownThread(thread *t,lua_State *lua,int cleanup);
const char* getThreadName(thread *t); // can be NULL

// SCRIPT UTILITY
#define getScriptName(script) ((const char*)((script)+1))
void getScriptUserdata(lua_State *lua,script *s); // pushes userdata
script* createScript(script_collection *c,int init,int envarg,struct dsl_file *file,const char *name,lua_State *lua,int *destroyedwhilerunning); // if there's an error, an error is on the lua stack (nil error if it skipped auto start)
int importScript(script_manager *sm,struct dsl_file *file,const char *name,lua_State *lua); // if error, error is pushed on stack
void destroyScript(script *s,lua_State *lua,int cleanup);
int shutdownScript(script *s,lua_State *lua,int cleanup);

// COLLECTION UTILITY
#define getScriptCollectionName(collection) (collection->name)
#define getScriptCollectionPrefix(collection) ((const char*)((collection)+1))
script_collection* createScriptCollection(script_manager *sm,const char *name,const char *path,loader_collection *ptr); // path is simply a prefix for other scripts
void destroyScriptCollection(script_collection *sc,lua_State *lua,int cleanup);
int shutdownScriptCollection(script_collection *sc,lua_State *lua,int cleanup);

// MANAGER CONTROLLER
script_manager* createScriptManager(struct dsl_state *dsl);
void destroyScriptManager(script_manager *sm);
void updateScriptManagerInit(script_manager *sm,lua_State *lua); // gamemain
void updateScriptManagerUpdate(script_manager *sm,lua_State *lua,int type);

#ifdef __cplusplus
}
#endif

// TYPE DEFINITIONS
struct script_manager{
	struct dsl_state *dsl;
	int use_base_funcs;
	int running_script_update;
	struct script_collection *running_collection;
	struct script *running_script;
	struct thread *running_thread;
	struct script_collection *collections;
};
struct script_collection{
	// LUA_REGISTRY[script_collection*] = {} (created when requested by a script, destroyed w/ the script_collection)
	// a null terminated string follows this struct with the script_collection prefix
	struct script_manager *manager;
	struct script *scripts;
	loader_collection *lc;
	char *name;
	int net; // lua reference to a shared net table
	int flags;
	int running;
	struct script_collection *next;
};
struct script{
	// LUA_REGISTRY[script*] = {} (for whole existance of script, holds the script environment)
	// a null terminated string follows this struct with the script name
	struct script_collection *collection;
	struct thread *threads[TOTAL_THREAD_TYPES];
	#ifndef DSL_SERVER_VERSION
	char script_object[SCRIPT_BYTES];
	#endif
	int thread_count; // total count just for checking if the script is needed
	int userdata; // lua object that holds the script
	int hash;
	int flags;
	int running;
	struct script *next;
};
struct thread{
	// LUA_REGISTRY[lua_thread] = thread* and [thread*] = lua_thread (for whole existance of the thread)
	// a null terminated string follows this struct with the thread name *if* the NAMED_THREAD flag is on
	struct script *script;
	lua_State *coroutine;
	unsigned when;
	int type;
	int flags;
	int running;
	struct thread *next;
};

#endif