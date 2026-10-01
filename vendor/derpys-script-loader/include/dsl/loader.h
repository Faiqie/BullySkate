/* DERPY'S SCRIPT LOADER: SCRIPT LOADER
	
	the file loader finds scripts and controls them using the manager
	it also handles general file access to support any way a collection may load files
	
*/

#ifndef DSL_LOADER_H
#define DSL_LOADER_H

#include "network.h"

struct config_file;
struct dsl_state;
struct loader_collection;
struct script_collection;

// LOADER LIMITS
#define MAX_LOADER_PATH 128 // collection name, slash, file path, and NUL

// LOADER COLLECTION FLAGS
#define LOADER_EXISTS 1
#define LOADER_FOLDER 2
#define LOADER_FAILED 4 // if an "actual" failure happened when started
#define LOADER_STARTING 8 // only to keep dependencies from including each other
#define LOADER_STOPPING 16
#define LOADER_RESTARTING 32
#define LOADER_ZIPWARNING 64 // a zip contains a folder with the same name in the root
#define LOADER_SCRIPTLESS 128
#define LOADER_CLIENTFILES 256 // has client files
#define LOADER_NETBLOCKER 512 // must be stopped for server scripts to be prepared
#define LOADER_NETWORK 1024 // from a server
#define LOADER_LOCAL 2048 // not from a server
#define LOADER_DUPLICATE 4096 // duplicate zip (only updated when starting)
#define LOADER_ALLOWSERVER 8192 // allow_on_server
#define LOADER_NETRESTARTED 16384 // don't "ensure" this collection because it was just restarted for the network
#define LOADER_DIDNTAUTOSTART 32768 // just to not show FAILED when a script used DontAutoStartScript

// TYPES
typedef struct loader_collection{
	struct loader_collection *next;
	struct script_collection *sc; // NULL when not running or stopping
	struct config_file *cfg;
	struct config_file *sc_cfg; // a copy of cfg when sc is started and that stays until sc stops
	int sc_cfg_ref; // lua userdata initialized to LUA_NOREF and only used by the config lib
	int zip_files;
	zip_t *zip;
	int flags;
	#ifdef _WIN32
	char prefix[MAX_PATH]; // added in version 7 or it would have followed the structure
	#else
	char prefix[PATH_MAX];
	#endif
	#ifdef DSL_SERVER_VERSION
	int players[NET_MAX_PLAYER_COUNT/(sizeof(int)*8)+!!(NET_MAX_PLAYER_COUNT%(sizeof(int)*8))]; // only valid when running players that need to update (set on by loader.c, set off by sv_server.c)
	#endif
}loader_collection;
typedef struct script_loader{
	loader_collection *first;
	struct dsl_state *dsl;
}script_loader;
typedef struct dsl_file{
	loader_collection *lc;
	union{
		zip_file_t *zfile; // lc != NULL
		FILE *file;
	};
}dsl_file;

#ifdef __cplusplus
extern "C" {
#endif

// FILE ACCESS
#define isDslFileInZip(f) (f->lc != NULL)
dsl_file* openDslFile(struct dsl_state *dsl,loader_collection *lc,const char *name,const char *mode,long *size); // size result is optional
long readDslFile(dsl_file *f,char *buffer,long bytes);
long writeDslFile(dsl_file *f,const char *buffer,long bytes);
void closeDslFile(dsl_file *f);
const char* getDslFileError(void);

// LOADER STATE
script_loader* createScriptLoader(struct dsl_state *state);
void startScriptLoader(struct dsl_state *dsl,script_loader *sl);
void destroyScriptLoader(script_loader* state);

// LOADER API
int stopScriptLoaderCollection(script_loader *sl,struct script_collection *sc);
int startScriptLoaderDependency(script_loader *sl,struct script_collection *sc,const char *dep);
#ifdef DSL_SERVER_VERSION
int getScriptLoaderPlayerBit(loader_collection *lc,unsigned id);
void setScriptLoaderPlayerBit(loader_collection *lc,unsigned id,int on);
void setScriptLoaderPlayerBits(script_loader *sl,unsigned id,int on);
#else
void prepareScriptLoaderForServer(script_loader *sl);
void refreshScriptLoaderServerScripts(script_loader *sl,unsigned count,char **active);
void ensureScriptLoaderServerScripts(script_loader *sl,unsigned count,char **active);
void restartScriptLoaderServerScript(script_loader *sl,unsigned count,char **active,const char *name);
void killScriptLoaderServerScripts(script_loader *sl,const char *only);
#endif

#ifdef __cplusplus
}
#endif

#endif