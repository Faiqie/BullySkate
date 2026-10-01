/* DERPY'S SCRIPT LOADER: COMMAND PROCESSOR
	
	this is where commands are registered and processed
	
*/

#ifndef DSL_COMMAND_H
#define DSL_COMMAND_H

struct dsl_state;

// TYPES
typedef struct script_command *script_cmdlist;
typedef void (*script_cmdfunc)(void *arg,int argc,char **argv);

#ifdef __cplusplus
extern "C" {
#endif

// COMMAND SET
#define setScriptCommand(list,name,func,arg) setScriptCommandEx(list,name,NULL,func,arg,0)
int setScriptCommandEx(script_cmdlist *list,const char *name,const char *help,script_cmdfunc callback,void *arg,int raw); // pass NULL to remove, returns non-zero on success (only fails for mem allocation though)

// COMMAND UTILITY
int doesScriptCommandExist(script_cmdlist*,const char*);
int areScriptCommandNamesEqual(const char*,const char*);

// COMMAND CONTROLLER
script_cmdlist* createScriptCommandList(struct dsl_state*);
void destroyScriptCommandList(script_cmdlist*);
int processScriptCommandLine(script_cmdlist*,const char*);

#ifdef __cplusplus
}
#endif

#endif