/* DERPY'S SCRIPT LOADER: EVENT HANDLER
	
	the event handler is made pretty general purpose for all sorts of events and cleanup
	there are 2 types of events, ones for C that have a unique ID and ones for LUA that use a string
	the C events are often used for cleanup, and some built-in LUA events are used for letting scripts modify certain game behaviors
	scripts can also make and trigger their own events for whatever reasons they see fit
	
*/

#include <dsl/dsl.h>
#include <string.h>

#define ARRAY_INCREMENT 1

// TYPES
typedef struct c_callback{
	script_event_cb func;
	void *arg;
}c_callback;
typedef struct c_event{
	c_callback *array;
	int count;
	int size;
}c_event;
typedef struct lua_callback{
	struct lua_callback *next;
	int userdata; // lua userdata holding a pointer to lua_callback
	int running; // counter keeping track of if it is running
	int ref; // lua function (LUA_NOREF = dead)
}lua_callback;
typedef struct lua_script{
	struct lua_script *next;
	lua_callback *callbacks[LUA_EVENT_TYPES];
	script *s;
	int running;
	int dead;
}lua_script;
typedef struct lua_event{
	struct lua_event *next;
	lua_script *scripts;
	int running;
}lua_event; // string follows struct
typedef struct script_events{
	c_event c_events[EVENT_TOTAL_COUNT]; // each event is an array of callbacks for C usage
	lua_event *lua_events;
}script_events;

// C
int addScriptEventCallback(script_events *se,int event,script_event_cb func,void *user_arg){ // zero if failed
	c_event *events;
	c_callback *array;
	
	events = se->c_events + event;
	array = events->array;
	if(events->count == events->size){
		array = realloc(array,sizeof(c_event)*(events->size+ARRAY_INCREMENT));
		if(!array)
			return 0;
		events->array = array;
		events->size += ARRAY_INCREMENT;
	}
	array += events->count++;
	array->func = func;
	array->arg = user_arg;
	return 1;
}
int runScriptEvent(script_events *se,lua_State *lua,int event,void *event_arg){ // zero if any cb returned non-zero
	c_event *events;
	c_callback *array;
	int result;
	int count;
	
	events = se->c_events + event;
	array = events->array;
	result = 1;
	for(count = events->count;count;count--,array++)
		if((*array->func)(lua,array->arg,event_arg))
			result = 0;
	return result;
}

// LUA - COLLECT
static int isLuaScriptEmpty(lua_script *ls){
	int type;
	
	for(type = 0;type < LUA_EVENT_TYPES;type++)
		if(ls->callbacks[type])
			return 0;
	return 1;
}
static void destroyLuaEvent(script_events *se,lua_event *le){
	lua_event *search,*next;
	
	search = se->lua_events;
	if(search == le)
		se->lua_events = le->next;
	else
		for(;next = search->next;search = next)
			if(next == le){
				search->next = le->next;
				break;
			}
	free(le);
}
static void destroyLuaScript(lua_event *le,lua_script *ls){
	lua_script *search,*next;
	
	search = le->scripts;
	if(search == ls)
		le->scripts = ls->next;
	else
		for(;next = search->next;search = next)
			if(next == ls){
				search->next = ls->next;
				break;
			}
	free(ls);
}
static void destroyLuaCallback(lua_script *ls,lua_callback *cb,lua_State *lua,int type){
	lua_callback *search,*next;
	
	search = ls->callbacks[type];
	if(search == cb)
		ls->callbacks[type] = cb->next;
	else
		for(;next = search->next;search = next)
			if(next == cb){
				search->next = cb->next;
				break;
			}
	luaL_unref(lua,LUA_REGISTRYINDEX,cb->userdata);
	// cb->ref will have already been unref'd by this point (because that is how it is marked dead)
	free(cb);
}
static void collectLuaScriptEvents(script_events *se,lua_State *lua){
	lua_event *le,*len;
	lua_script *ls,*lsn;
	lua_callback *cb,*cbn;
	int type;
	
	for(le = se->lua_events;le;le = len){
		for(ls = le->scripts;ls;ls = lsn){
			for(type = 0;type < LUA_EVENT_TYPES;type++)
				for(cb = ls->callbacks[type];cb;cb = cbn){
					cbn = cb->next;
					if(cb->ref == LUA_NOREF && !cb->running)
						destroyLuaCallback(ls,cb,lua,type);
				}
			lsn = ls->next;
			if(isLuaScriptEmpty(ls) && !ls->running)
				destroyLuaScript(le,ls);
		}
		len = le->next;
		if(!le->scripts && !le->running)
			destroyLuaEvent(se,le);
	}
}

// LUA - ADD
static int getLuaCallbackString(lua_State *lua){
	lua_callback *cb;
	char buffer[32];
	
	if(cb = *(lua_callback**)lua_touserdata(lua,1))
		sprintf(buffer,"event handler: %p",cb);
	else
		sprintf(buffer,"invalid event handler");
	lua_pushstring(lua,buffer);
	return 1;
}
static lua_event* getLuaEvent(script_events *se,const char *name){ // NULL if failed
	lua_event *le;
	
	for(le = se->lua_events;le;le = le->next)
		if(!strcmp((char*)(le+1),name))
			return le;
	if(le = calloc(1,sizeof(lua_event)+strlen(name)+1)){
		le->next = se->lua_events;
		se->lua_events = le;
		strcpy((char*)(le+1),name);
	}
	return le;
}
static lua_script* getLuaScript(lua_event *le,script *s){ // NULL if failed
	lua_script *ls;
	
	for(ls = le->scripts;ls;ls = ls->next)
		if(!ls->dead && ls->s == s) // ignore dead scripts (their s is garbage)
			return ls;
	if(ls = calloc(1,sizeof(lua_script))){
		ls->next = le->scripts;
		le->scripts = ls;
		ls->s = s;
	}
	return ls;
}
static int addLuaCallback(lua_script *ls,lua_State *lua,int type){ // pops function, pushes userdata, zero if failed
	lua_callback *cb,*next,*created;
	
	created = calloc(1,sizeof(lua_callback));
	if(!created)
		return 0;
	created->ref = luaL_ref(lua,LUA_REGISTRYINDEX);
	*(lua_callback**)lua_newuserdata(lua,sizeof(lua_callback*)) = created;
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getLuaCallbackString);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lua_pushvalue(lua,-1);
	created->userdata = luaL_ref(lua,LUA_REGISTRYINDEX);
	if(cb = ls->callbacks[type]){
		while(next = cb->next)
			cb = next;
		cb->next = created;
	}else
		ls->callbacks[type] = created;
	return 1;
}
int addLuaScriptEventCallback(script_events *se,script *s,lua_State *lua,int type,const char *name){ // pops function regardless, pushes userdata if non-zero, zero if failed, NULL is allowed for script
	lua_script *ls;
	lua_event *le;
	
	if((le = getLuaEvent(se,name)) && (ls = getLuaScript(le,s)) && addLuaCallback(ls,lua,type))
		return 1;
	lua_pop(lua,1);
	collectLuaScriptEvents(se,lua);
	return 0;
}

// LUA - REMOVE
void removeLuaScriptEventCallback(script_events *se,lua_State *lua){ // pops userdata
	lua_event *le;
	lua_script *ls;
	lua_callback *cb,*remove,**userdata;
	int type;
	
	if(userdata = lua_touserdata(lua,-1)){
		remove = *userdata;
		for(le = se->lua_events;le;le = le->next)
			for(ls = le->scripts;ls;ls = ls->next)
				for(type = 0;type < LUA_EVENT_TYPES;type++)
					for(cb = ls->callbacks[type];cb;cb = cb->next)
						if(cb == remove){
							*userdata = NULL;
							lua_pop(lua,1);
							luaL_unref(lua,LUA_REGISTRYINDEX,cb->ref);
							cb->ref = LUA_NOREF;
							collectLuaScriptEvents(se,lua);
							return;
						}
	}
	lua_pop(lua,1);
}

// LUA - RUN
static void printLuaError(lua_State *lua,dsl_state *dsl){ // reads error but pops nothing
	if(lua_isstring(lua,-1))
		printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),lua_tostring(lua,-1));
	else
		printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),TEXT_FAIL_UNKNOWN);
}
static int callLuaCallback(script *s,lua_callback *cb,lua_State *lua,int args){ // reads args but pops nothing, non-zero if cb returned true
	script_block sb;
	dsl_state *dsl;
	int result;
	int base;
	int arg;
	
	dsl = getDslState(lua,0);
	if(!dsl || !lua_checkstack(lua,args+LUA_MINSTACK))
		return 0;
	result = 0;
	base = lua_gettop(lua) - args;
	lua_rawgeti(lua,LUA_REGISTRYINDEX,cb->ref);
	for(arg = 1;arg <= args;arg++)
		lua_pushvalue(lua,base+arg);
	startScriptBlock(dsl->manager,s,&sb);
	if(lua_pcall(lua,args,1,0))
		printLuaError(lua,dsl);
	else if(lua_toboolean(lua,-1))
		result = 1;
	finishScriptBlock(dsl->manager,&sb,lua);
	lua_pop(lua,1);
	return result;
}
int runLuaScriptEvent(script_events *se,lua_State *lua,int type,const char *name,int args){ // reads args but pops nothing, zero if any cb returned true
	lua_event *le;
	lua_script *ls;
	lua_callback *cb;
	int result;
	
	result = 1;
	for(le = se->lua_events;le;le = le->next)
		if(!strcmp((char*)(le+1),name)){
			le->running++;
			for(ls = le->scripts;ls;ls = ls->next){
				ls->running++;
				for(cb = ls->callbacks[type];cb;cb = cb->next){
					cb->running++;
					if(cb->ref != LUA_NOREF && callLuaCallback(ls->s,cb,lua,args))
						result = 0;
					cb->running--;
				}
				ls->running--;
			}
			le->running--;
		}
	collectLuaScriptEvents(se,lua);
	return result;
}

// LUA - DEBUG
void debugLuaScriptEvents(script_events *se,lua_State *lua){ // pushes a table with debug info
	lua_event *le;
	lua_script *ls;
	lua_callback *cb;
	char buffer[1024];
	int type;
	int n;
	
	lua_newtable(lua);
	for(le = se->lua_events;le;le = le->next){
		lua_pushstring(lua,(char*)(le+1));
		lua_newtable(lua);
		for(ls = le->scripts;ls;ls = ls->next){
			if(ls->s)
				sprintf(buffer,"%p (%s)",ls->s,getScriptName(ls->s));
			else
				sprintf(buffer,"%p (NULL)",ls->s);
			lua_pushstring(lua,buffer);
			lua_newtable(lua);
			for(type = 0;type < LUA_EVENT_TYPES;type++){
				lua_newtable(lua);
				for(n = 0,cb = ls->callbacks[type];cb;cb = cb->next)
					if(cb->ref != LUA_NOREF){
						lua_rawgeti(lua,LUA_REGISTRYINDEX,cb->userdata);
						if(lua_isnil(lua,-1)){
							lua_pushstring(lua,"invalid");
							lua_replace(lua,-2);
						}
						lua_rawseti(lua,-2,++n);
					}
				lua_rawseti(lua,-2,type);
			}
			lua_rawset(lua,-3);
		}
		lua_rawset(lua,-3);
	}
}

// STATE
static int destroyedScript(lua_State *lua,script_events *se,script *s){
	lua_event *le;
	lua_script *ls;
	lua_callback *cb,**userdata;
	int type;
	
	for(le = se->lua_events;le;le = le->next)
		for(ls = le->scripts;ls;ls = ls->next)
			if(!ls->dead && ls->s == s){ // ignore dead scripts (their s is garbage)
				for(type = 0;type < LUA_EVENT_TYPES;type++)
					for(cb = ls->callbacks[type];cb;cb = cb->next)
						if(cb->ref != LUA_NOREF){
							lua_rawgeti(lua,LUA_REGISTRYINDEX,cb->userdata);
							if(userdata = lua_touserdata(lua,-1))
								*userdata = NULL;
							lua_pop(lua,1);
							luaL_unref(lua,LUA_REGISTRYINDEX,cb->ref);
							cb->ref = LUA_NOREF;
						}
				ls->dead = 1; // ignore this script from now on (it only still exists for graceful cleanup)
			}
	collectLuaScriptEvents(se,lua);
	return 0;
}
script_events* createScriptEvents(){ // NULL if failed
	script_events *se;
	
	se = calloc(1,sizeof(script_events));
	if(!se)
		return NULL;
	addScriptEventCallback(se,EVENT_SCRIPT_CLEANUP,&destroyedScript,se); // TODO: this getting called twice fucks with the !dead check
	return se;
}
void destroyScriptEvents(script_events *se){
	c_event *events;
	c_callback *array;
	lua_event *le,*lenext;
	lua_script *ls,*lsnext;
	lua_callback *cb,*cbnext;
	int index;
	
	events = se->c_events;
	for(index = 0;index < EVENT_TOTAL_COUNT;index++)
		if(array = events[index].array)
			free(array);
	for(le = se->lua_events;le;le = lenext){
		for(ls = le->scripts;ls;ls = lsnext){
			for(index = 0;index < LUA_EVENT_TYPES;index++)
				for(cb = ls->callbacks[index];cb;cb = cbnext){
					cbnext = cb->next;
					free(cb);
				}
			lsnext = ls->next;
			free(ls);
		}
		lenext = le->next;
		free(le);
	}
	free(se);
}