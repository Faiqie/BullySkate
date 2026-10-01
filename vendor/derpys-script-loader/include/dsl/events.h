/* DERPY'S SCRIPT LOADER: EVENT HANDLER
	
	the event handler is made pretty general purpose for all sorts of events and cleanup
	there are 2 types of events, ones for C that have a unique ID and ones for LUA that use a string
	the C events are often used for cleanup, and some built-in LUA events are used for letting scripts modify certain game behaviors
	scripts can also make and trigger their own events for whatever reasons they see fit
	
*/

#ifndef DSL_EVENTS_H
#define DSL_EVENTS_H

#include "manager.h"

// C EVENTS
#define EVENT_MANAGER_UPDATE 0 // int | thread type after main manager updates
#define EVENT_KEY_DOWN_UPDATE 1 // int | directinput keyboard scan code
#define EVENT_THREAD_UPDATE 2 // thread* | before thread update
#define EVENT_SCRIPT_CREATED 3 // script* | before it runs env is -1
#define EVENT_SCRIPT_CLEANUP 4 // script* | not run when dsl shuts down
#define EVENT_COLLECTION_DESTROYED 5 //script_collection* | not run when dsl shuts down
#define EVENT_TOTAL_COUNT 6

// LUA EVENTS
#define LOCAL_EVENT 0
#define REMOTE_EVENT 1
#define LUA_EVENT_TYPES 2

// TYPES
typedef struct script_events script_events;
typedef int (*script_event_cb)(lua_State *lua,void *user_arg,void *event_arg);

#ifdef __cplusplus
extern "C" {
#endif

// C
int addScriptEventCallback(script_events *se,int event,script_event_cb func,void *user_arg); // zero if failed
int runScriptEvent(script_events *se,lua_State *lua,int event,void *event_arg); // zero if any cb returned non-zero

// LUA
int addLuaScriptEventCallback(script_events *se,script *s,lua_State *lua,int type,const char *name); // pops function regardless, pushes userdata if non-zero, zero if failed, NULL is allowed for script
void removeLuaScriptEventCallback(script_events *se,lua_State *lua); // pops userdata
int runLuaScriptEvent(script_events *se,lua_State *lua,int type,const char *name,int args); // reads args but pops nothing, zero if any cb returned true

// DEBUG
void debugLuaScriptEvents(script_events *se,lua_State *lua); // pushes a table with debug info

// STATE
script_events *createScriptEvents(void);
void destroyScriptEvents(script_events*);

#ifdef __cplusplus
}
#endif

#endif