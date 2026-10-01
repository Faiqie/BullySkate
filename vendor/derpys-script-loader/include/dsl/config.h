/* DERPY'S SCRIPT LOADER: CONFIG PARSER
	
	parses config files in a pretty basic way
	blank lines are ignored and # marks the rest of a line as a comment
	config options are specified in key value pairs with whitespace in between
	key names are case sensitive and value types are only defined by their usage
	the same key name can be given multiple times if desired for a list of values
	":" / "=" are allowed between keys and values and "[" on the start of a line ignores it
	
*/

#ifndef DSL_CONFIG_H
#define DSL_CONFIG_H

struct dsl_state;
struct loader_collection;

#define CONFIG_EMPTY 0
#define CONFIG_MISSING 1
#define CONFIG_FAILURE 2 // memory

typedef struct config_file config_file;

#ifdef __cplusplus
extern "C" {
#endif

#define loadConfigSettings(filename,failreason) loadConfigSettingsEx(NULL,NULL,filename,failreason)
config_file* loadConfigSettingsEx(struct dsl_state *dsl,struct loader_collection *lc,const char *filename,int *failreason); // returns NULL if missing or empty
config_file* copyConfigSettings(config_file *cfg); // cfg must not be NULL, only fails for memory error
void freeConfigSettings(config_file *cfg);
int getConfigValueCount(config_file *cfg,const char *key);
#define getConfigValue(ptr,key) getConfigValueArray(ptr,key,0)
#define getConfigString(ptr,key) getConfigStringArray(ptr,key,0)
#define getConfigBoolean(ptr,key) getConfigBooleanArray(ptr,key,0)
#define getConfigInteger(ptr,key) getConfigIntegerArray(ptr,key,0)
const char* getConfigValueArray(config_file *cfg,const char *key,int index); // returns raw string (or NULL if it doesn't exist)
const char* getConfigStringArray(config_file *cfg,const char *key,int index); // returns string with stripped quotes (or NULL)
int getConfigBooleanArray(config_file *cfg,const char *key,int index); // returns zero if missing or false (see config.c)
int getConfigIntegerArray(config_file *cfg,const char *key,int index); // returns zero if invalid number

#ifdef __cplusplus
}
#endif

#endif