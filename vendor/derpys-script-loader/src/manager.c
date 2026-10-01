/* DERPY'S SCRIPT LOADER: SCRIPT MANAGER
	
	this is where lua scripts and threads are initialized and controlled
	the manager state is created by dsl.c and used for loading by loader.c
	threads belong to scripts that belong to script collections that belong to the script manager
	
*/

#include <dsl/dsl.h>
#include <string.h>

#define USE_FAKE_THREADS
#define GC_MAX_THRESHOLD 131072 // 128 MB, sometimes bully's threshold gets out of control

// Types.
struct chunk_loader{
	dsl_file *file;
	char data[BUFSIZ];
	int hash;
	int warn;
	int oncr;
	int bin;
};

// Miscellaneous.
static int getScriptString(lua_State *lua){
	script *s;
	
	if(s = *(script**)lua_touserdata(lua,1))
		lua_pushfstring(lua,"script: %s",(char*)(s+1));
	else
		lua_pushstring(lua,"invalid script");
	return 1;
}
void getScriptUserdata(lua_State *lua,script *s){
	if(s->userdata == LUA_NOREF){
		*(script**)lua_newuserdata(lua,sizeof(script*)) = s;
		lua_newtable(lua);
		lua_pushstring(lua,"__tostring");
		lua_pushcfunction(lua,&getScriptString);
		lua_rawset(lua,-3);
		lua_setmetatable(lua,-2);
		lua_pushvalue(lua,-1);
		s->userdata = luaL_ref(lua,LUA_REGISTRYINDEX);
	}else
		lua_rawgeti(lua,LUA_REGISTRYINDEX,s->userdata);
}

// Reader.
static void hashScript(struct chunk_loader *loader,size_t bytes){
	const char *str;
	int x;
	
	str = loader->data;
	x = loader->hash;
	while(bytes){
		x *= 0x83;
		x += str[--bytes];
	}
	loader->hash = x;
}
static int detectConflictingLinebreaks(struct chunk_loader *loader,size_t bytes){
	char *buffer;
	size_t i;
	
	buffer = loader->data;
	for(i = 0;i < bytes;i++)
		if(loader->oncr){
			if(buffer[i] != '\n')
				return 1;
			loader->oncr = 0;
		}else if(buffer[i] == '\r')
			loader->oncr = 1;
	return 0;
}
static const char* loadScriptReader(lua_State *lua,struct chunk_loader *loader,size_t *bytesread){
	if(*bytesread = readDslFile(loader->file,loader->data,BUFSIZ)){
		hashScript(loader,*bytesread);
		if(loader->warn && detectConflictingLinebreaks(loader,*bytesread))
			loader->warn = 0;
		if(!loader->bin)
			loader->bin = *(loader->data) == '\033' ? 1 : 2;
		return loader->data;
	}
	return NULL;
}
static int loadScript(lua_State *lua,dsl_file *file,const char *name,dsl_state *dsl,script *s){ // zero = success
	struct chunk_loader *loader;
	int status;
	
	loader = malloc(sizeof(struct chunk_loader) + strlen(name) + 2);
	if(!loader){
		lua_pushstring(lua,"couldn't allocate memory for script loader");
		return LUA_ERRMEM;
	}
	loader->file = file;
	loader->hash = 0;
	loader->warn = 1;
	loader->oncr = 0;
	loader->bin = 0; // 0 at first, 1 if binary or 2 if not
	*(char*)(loader+1) = '@';
	strcpy((char*)(loader+1)+1,name);
	status = lua_load(lua,&loadScriptReader,loader,(char*)(loader+1));
	if(s){
		s->flags |= HASHED_SCRIPT;
		s->hash = loader->hash;
	}
	if(!loader->warn && loader->bin != 1)
		printConsoleWarning(dsl->console,"%sdetected macintosh style linebreaks in \"%s\" which will break line number detection, consider fixing this using EOL conversion in notepad++",getDslPrintPrefix(dsl,0),name);
	free(loader);
	return status;
}

// Threads.
static void addThreadToList(thread *t,thread **list){
	thread *last,*next;
	
	if(last = *list){
		while(next = last->next)
			last = next;
		last->next = t;
	}else
		*list = t;
}
static void removeThreadFromList(thread *t,thread **list){
	thread *iter,*next;
	
	if((iter = *list) == t)
		*list = t->next;
	else
		for(;next = iter->next;iter = next)
			if(next == t){
				iter->next = t->next;
				return;
			}
}
static thread* activateThread(script *s,lua_State *lua,int type,int flags,const char *name){ // creates a thread if a function with name is found in the script environment
	thread *t;
	
	if(s->flags & DISABLE_SCRIPT_FLOW)
		return NULL;
	t = NULL;
	lua_pushlightuserdata(lua,s);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	if(lua_istable(lua,-1)){
		lua_pushstring(lua,name);
		lua_rawget(lua,-2);
		lua_replace(lua,-2);
		if(lua_isfunction(lua,-1) && !lua_iscfunction(lua,-1))
			t = createThread(s,lua,0,type,flags,name);
	}
	lua_pop(lua,1);
	return t;
}
thread* createThread(script *s,lua_State *lua,int nargs,int type,int flags,const char *name){ // 1 function & nargs are popped, if successful a thread is pushed (otherwise nil is pushed)
	thread *t;
	
	if(name)
		t = malloc(sizeof(thread) + strlen(name) + 1);
	else
		t = malloc(sizeof(thread));
	if(!t){
		lua_pop(lua,nargs+1);
		lua_pushnil(lua);
		return NULL;
	}else if(s->flags & SHUTDOWN_SCRIPT)
		flags |= CLEANUP_THREAD;
	t->script = s;
	t->coroutine = lua_newthread(lua);
	t->when = 0;
	t->type = type;
	t->flags = flags;
	t->running = 0;
	t->next = NULL;
	if(nargs)
		t->flags |= PASS_ARGS_TO_THREAD;
	if(name){
		strcpy((char*)(t+1),name);
		t->flags |= NAMED_THREAD;
	}
	lua_insert(lua,-nargs-2); // put thread before everything else
	lua_xmove(lua,t->coroutine,nargs+1);
	lua_pushlightuserdata(lua,t);
	lua_pushvalue(lua,-2);
	lua_rawset(lua,LUA_REGISTRYINDEX); // registry[t*] = thread
	lua_pushvalue(lua,-1);
	lua_pushlightuserdata(lua,t);
	lua_rawset(lua,LUA_REGISTRYINDEX); // registry[thread] = t*
	addThreadToList(t,s->threads+type);
	s->thread_count++;
	return t;
}
void destroyThread(thread *t,lua_State *lua,int cleanup){
	script *s;
	int flags;
	
	s = t->script;
	flags = t->flags;
	removeThreadFromList(t,s->threads+t->type);
	s->thread_count--;
	if(cleanup){
		lua_pushlightuserdata(lua,t);
		lua_rawget(lua,LUA_REGISTRYINDEX);
		if(lua_type(lua,-1) == LUA_TTHREAD){
			lua_pushnil(lua);
			lua_rawset(lua,LUA_REGISTRYINDEX); // registry[thread] = nil
		}else
			lua_pop(lua,1); // should never happen but failsafe i guess
		lua_pushlightuserdata(lua,t);
		lua_pushnil(lua);
		lua_rawset(lua,LUA_REGISTRYINDEX); // registry[t*] = nil
	}
	if(s->flags & SHUTDOWN_SCRIPT && !s->running && !s->thread_count)
		destroyScript(s,lua,1);
	free(t);
}
int shutdownThread(thread *t,lua_State *lua,int cleanup){
	script *s;
	int keep;
	
	s = t->script;
	if(cleanup && ~s->flags & SHUTDOWN_SCRIPT && ~t->flags & SHUTDOWN_THREAD){
		if(t->flags & START_MAIN_AFTER_THREAD){
			if(t->flags & ERROR_RUNNING_THREAD)
				shutdownScript(s,lua,1); // if setup fails, shutdown script
			else
				activateThread(s,lua,GAME_THREAD,0,"main");
		}else if(t->flags & QUIT_SCRIPT_AFTER_THREAD)
			shutdownScript(s,lua,1);
	}
	if(keep = t->running)
		t->flags |= SHUTDOWN_THREAD;
	else
		destroyThread(t,lua,1);
	return !keep;
}
const char* getThreadName(thread *t){
	if(t->flags & NAMED_THREAD)
		return (char*)(t+1);
	return NULL;
}

// Scripts.
#ifndef DSL_SERVER_VERSION
#ifdef USE_FAKE_THREADS
static void setScriptObjectVirtualThread(char *so,lua_State *lt){
	if(*(int*)(so+0x1148) = lt != NULL){ // count
		*(lua_State**)(so+0x48) = lt; // thread
		*(int*)(so+0x114C) = 0; // current
	}else
		*(int*)(so+0x114C) = -1;
}
#endif
#endif
static void makeScriptEnvironment(lua_State *lua){
	lua_newtable(lua);
	lua_newtable(lua);
	lua_pushstring(lua,"__index");
	lua_pushvalue(lua,LUA_GLOBALSINDEX);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
}
static int runScript(script *s,int init,lua_State *lua){
	int status;
	
	lua_pushvalue(lua,-2);
	lua_setfenv(lua,-2);
	if(init)
		s->flags |= INITIAL_SCRIPT_RUN;
	status = lua_pcall(lua,0,0,0);
	if(init)
		s->flags &= ~INITIAL_SCRIPT_RUN;
	return status;
}
static void addScriptToCollection(script *s,script_collection *c){
	script *last,*next;
	
	if(last = c->scripts){
		while(next = last->next)
			last = next;
		last->next = s;
	}else
		c->scripts = s;
}
static void removeScriptFromCollection(script *s,script_collection *c){
	script *iter,*next;
	
	if((iter = c->scripts) == s)
		c->scripts = s->next;
	else
		for(;next = iter->next;iter = next)
			if(next == s){
				iter->next = s->next;
				return;
			}
}
script* createScript(script_collection *c,int init,int envarg,dsl_file *file,const char *name,lua_State *lua,int *destroyedwhilerunning){ // if there's an error, an error is on the lua stack (nil error if it skipped auto start)
	script_manager *sm;
	script_block sb;
	script *s;
	
	sm = c->manager;
	s = malloc(sizeof(script) + strlen(name) + 1);
	if(destroyedwhilerunning)
		*destroyedwhilerunning = 0;
	if(!s){
		lua_pushstring(lua,"memory allocation failed");
		if(!file)
			lua_pop(lua,1);
		return NULL;
	}else if(c->flags & SHUTDOWN_COLLECTION){
		lua_pushstring(lua,"script_collection is shutting down");
		free(s);
		if(!file)
			lua_pop(lua,1);
		return NULL;
	}
	s->collection = c;
	s->thread_count = 0;
	memset(s->threads,0,sizeof(thread*)*TOTAL_THREAD_TYPES);
	#ifndef DSL_SERVER_VERSION
	memset(s->script_object,0,SCRIPT_BYTES); // might not be needed but its safer
	setupGameScript(s->script_object,"dsl_script");
	#endif
	s->userdata = LUA_NOREF;
	s->hash = 0;
	s->flags = file ? 0 : DISABLE_SCRIPT_FLOW;
	s->running = 0;
	s->next = NULL;
	strcpy((char*)(s+1),name);
	addScriptToCollection(s,c);
	if(!file)
		lua_getfenv(lua,-1); // a NULL file makes a "virtual" script given a function and keeps its environment
	else if(!envarg) // environment was pushed?
		makeScriptEnvironment(lua);
	lua_pushlightuserdata(lua,s);
	lua_pushvalue(lua,-2);
	lua_rawset(lua,LUA_REGISTRYINDEX); // registry[s*] = env
	runScriptEvent(sm->dsl->events,lua,EVENT_SCRIPT_CREATED,s);
	startScriptBlock(sm,s,&sb);
	if(!file)
		lua_insert(lua,-2); // put virtual script environment before the function
	if((file && loadScript(lua,file,name,sm->dsl,s)) || runScript(s,init,lua)){
		lua_replace(lua,-2);
		shutdownScript(s,lua,0);
		finishScriptBlock(sm,&sb,lua);
		return NULL;
	}
	lua_pop(lua,1);
	if(!activateThread(s,lua,GAME_THREAD,START_MAIN_AFTER_THREAD,"MissionSetup"))
		activateThread(s,lua,GAME_THREAD,0,"main");
	if(finishScriptBlock(sm,&sb,lua)){
		lua_pushstring(lua,"script was destroyed while running");
		if(destroyedwhilerunning)
			*destroyedwhilerunning = 1;
		return NULL;
	}
	return s;
}
int importScript(script_manager *sm,dsl_file *file,const char *name,lua_State *lua){ // if error, error is pushed on stack
	lua_Debug ar;
	script *s;
	
	s = sm->running_script;
	if(!s){
		lua_pushstring(lua,"no script running");
		return 0;
	}
	lua_pushlightuserdata(lua,s);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	if(lua_istable(lua,-1) && (loadScript(lua,file,name,sm->dsl,NULL) || runScript(s,0,lua))){
		lua_replace(lua,-2); // replace environment with error
		return 0;
	}
	lua_pop(lua,1); // pop environment
	return 1;
}
void destroyScript(script *s,lua_State *lua,int cleanup){
	script_collection *c;
	script_manager *sm;
	script_block sb;
	thread *destroy;
	dsl_state *dsl;
	script **sptr;
	int i;
	
	c = s->collection;
	s->flags &= ~SHUTDOWN_SCRIPT;
	removeScriptFromCollection(s,c);
	for(i = 0;i < TOTAL_THREAD_TYPES;i++)
		while(destroy = s->threads[i])
			destroyThread(destroy,lua,cleanup);
	if(cleanup){
		sm = c->manager;
		dsl = sm->dsl;
		#ifndef DSL_SERVER_VERSION
		#ifdef USE_FAKE_THREADS
		setScriptObjectVirtualThread(s->script_object,NULL);
		#endif
		if(dsl->game)
			cleanupGameScript(s->script_object);
		#endif
		runScriptEvent(dsl->events,lua,EVENT_SCRIPT_CLEANUP,s);
		getScriptUserdata(lua,s);
		if(sptr = lua_touserdata(lua,-1))
			*sptr = NULL; // now this userdata points to an invalid script
		luaL_unref(lua,LUA_REGISTRYINDEX,s->userdata);
		startScriptBlock(sm,s,&sb); // this is just to make sure the script isn't fully destroyed during the event
		runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"ScriptDestroyed",1);
		finishScriptBlock(sm,&sb,lua);
		lua_pop(lua,1);
		lua_pushlightuserdata(lua,s);
		lua_pushnil(lua);
		lua_rawset(lua,LUA_REGISTRYINDEX); // registry[s*] = nil
		lua_setgcthreshold(lua,0); // cleanup after destroying script
	}
	if(c->flags & SHUTDOWN_COLLECTION && !c->running && !c->scripts)
		destroyScriptCollection(c,lua,1);
	free(s);
}
int shutdownScript(script *s,lua_State *lua,int cleanup){
	thread *shutdown,*next;
	script_manager *sm;
	script_block sb;
	dsl_state *dsl;
	int keep;
	
	if(~s->flags & SHUTDOWN_SCRIPT){
		sm = s->collection->manager;
		dsl = sm->dsl;
		getScriptUserdata(lua,s);
		startScriptBlock(sm,s,&sb); // this is just to make sure the script isn't fully destroyed during the event
		s->flags |= SHUTDOWN_SCRIPT; // so it can't recursively shut itself down
		runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"ScriptShutdown",1);
		s->flags &= ~SHUTDOWN_SCRIPT;
		finishScriptBlock(sm,&sb,lua);
		lua_pop(lua,1);
		for(keep = 0;keep < TOTAL_THREAD_TYPES;keep++)
			for(shutdown = s->threads[keep];shutdown;shutdown = next){
				next = shutdown->next;
				shutdownThread(shutdown,lua,0);
			}
		if(cleanup)
			activateThread(s,lua,GAME_THREAD,CLEANUP_THREAD,"MissionCleanup");
		runScriptEvent(dsl->events,lua,EVENT_SCRIPT_CLEANUP,s);
	}
	if(keep = s->running || s->thread_count)
		s->flags |= SHUTDOWN_SCRIPT;
	else
		destroyScript(s,lua,1);
	return !keep;
}

// Collections.
static void addCollectionToManager(script_collection *c,script_manager *sm){
	script_collection *last,*next;
	
	if(last = sm->collections){
		while(next = last->next)
			last = next;
		last->next = c;
	}else
		sm->collections = c;
}
static void removeCollectionFromManager(script_collection *c,script_manager *sm){
	script_collection *iter,*next;
	
	if((iter = sm->collections) == c)
		sm->collections = c->next;
	else
		for(;next = iter->next;iter = next)
			if(next == c){
				iter->next = c->next;
				return;
			}
}
static void reverseCollectionOrder(script_collection *c){
	script *a,*b,*next;
	
	if(a = c->scripts){
		b = a->next;
		a->next = NULL;
		while(b){
			next = b->next;
			b->next = a;
			a = b;
			b = next;
		}
		c->scripts = a;
	}
}
script_collection* createScriptCollection(script_manager *sm,const char *name,const char *path,loader_collection *ptr){
	script_collection *c;
	
	c = malloc(sizeof(script_collection) + strlen(path) + 1);
	if(!c)
		return NULL;
	c->manager = sm;
	c->scripts = NULL;
	c->lc = ptr;
	c->name = malloc(strlen(name) + 1);
	c->net = LUA_NOREF;
	c->flags = 0;
	c->running = 0;
	c->next = NULL;
	if(!c->name){
		free(c);
		return NULL;
	}
	strcpy(c->name,name);
	strcpy((char*)(c+1),path);
	addCollectionToManager(c,sm);
	return c;
}
void destroyScriptCollection(script_collection *c,lua_State *lua,int cleanup){
	script *destroy;
	
	c->flags &= ~SHUTDOWN_COLLECTION;
	removeCollectionFromManager(c,c->manager);
	while(destroy = c->scripts)
		destroyScript(destroy,lua,cleanup);
	if(cleanup){
		runScriptEvent(c->manager->dsl->events,lua,EVENT_COLLECTION_DESTROYED,c);
		if(c->net != LUA_NOREF)
			luaL_unref(lua,LUA_REGISTRYINDEX,c->net);
		lua_pushlightuserdata(lua,c);
		lua_pushnil(lua);
		lua_rawset(lua,LUA_REGISTRYINDEX); // registry[c*] = nil
	}
	free(c->name);
	free(c);
}
int shutdownScriptCollection(script_collection *c,lua_State *lua,int cleanup){
	script *shutdown,*next;
	int keep;
	
	if(~c->flags & SHUTDOWN_COLLECTION){
		reverseCollectionOrder(c); // so script cleanup unwinds instead of going in order
		for(shutdown = c->scripts;shutdown;shutdown = next){
			next = shutdown->next;
			shutdownScript(shutdown,lua,cleanup);
		}
	}
	if(keep = c->running || c->scripts)
		c->flags |= SHUTDOWN_COLLECTION;
	else
		destroyScriptCollection(c,lua,1);
	return !keep;
}

// Script manager.
script_manager* createScriptManager(dsl_state *dsl){
	script_manager *sm;
	
	sm = malloc(sizeof(script_manager));
	if(!sm)
		return NULL;
	sm->dsl = dsl;
	sm->running_script_update = 0;
	sm->running_collection = NULL;
	sm->running_script = NULL;
	sm->running_thread = NULL;
	sm->collections = NULL;
	return sm;
}
void destroyScriptManager(script_manager *sm){
	script_collection *destroy;
	
	while(destroy = sm->collections)
		destroyScriptCollection(destroy,NULL,0);
	free(sm);
}

// Script pool entry.
#ifndef DSL_SERVER_VERSION
static void setGameScriptObject(void *game,void *so){
	void **pool;
	int count;
	int index;
	
	pool = getGameScriptPool(game);
	count = getGameScriptCount(game);
	for(index = 0;index < count;index++)
		if(pool[index] == so)
			break;
	setGameScriptIndex(game,index);
}
static void addGameScriptObject(void *game,void *so){
	void **pool;
	int count;
	
	pool = getGameScriptPool(game);
	count = getGameScriptCount(game);
	setGameScriptCount(game,count+1);
	setGameScriptIndex(game,count);
	pool[count] = so;
}
static void removeGameScriptObject(void *game,void *so){
	void **pool;
	int count;
	int index;
	
	pool = getGameScriptPool(game);
	count = getGameScriptCount(game);
	for(index = 0;index < count;index++)
		if(pool[index] == so){
			setGameScriptCount(game,count-1);
			if(++index < count){
				for(;index < count;index++)
					pool[index-1] = pool[index];
				pool[index] = NULL;
			}else
				pool[index-1] = NULL; // nothing to slide down so just remove this one only
			return;
		}
}
#endif

// Script manager blocks.
static void startExecution(script_manager *sm,script_collection *c,script *s,thread *t){
	if(sm->running_collection = c)
		c->running++;
	if(sm->running_script = s)
		s->running++;
	if(sm->running_thread = t)
		t->running++;
	sm->use_base_funcs = 0;
}
static int finishExecution(script_manager *sm,script_collection *cc,script *cs,thread *ct,lua_State *lua,int usebase){
	script_collection *lc;
	script *ls;
	thread *lt;
	int remove;
	
	remove = 0;
	sm->use_base_funcs = usebase;
	if(lt = sm->running_thread)
		lt->running--;
	sm->running_thread = ct;
	if(lt && lt->flags & SHUTDOWN_THREAD && shutdownThread(lt,lua,1))
		remove++;
	if(ls = sm->running_script)
		ls->running--;
	sm->running_script = cs;
	if(ls && ls->flags & SHUTDOWN_SCRIPT && shutdownScript(ls,lua,1))
		remove++;
	if(lc = sm->running_collection)
		lc->running--;
	sm->running_collection = cc;
	if(lc && lc->flags & SHUTDOWN_COLLECTION && shutdownScriptCollection(lc,lua,1))
		remove++;
	return remove;
}
void startScriptBlock(script_manager *sm,script *s,script_block *backup){
	void *game;
	int index;
	
	backup->c = sm->running_collection;
	backup->s = sm->running_script;
	backup->t = sm->running_thread;
	backup->b = sm->use_base_funcs;
	startExecution(sm,s ? s->collection : NULL,s,NULL);
	#ifndef DSL_SERVER_VERSION
	if(game = sm->dsl->game){
		if(backup->s)
			removeGameScriptObject(game,backup->s->script_object);
		else
			backup->so = (index = getGameScriptIndex(game)) == -1 ? NULL : getGameScriptPool(game)[index];
		if(s)
			addGameScriptObject(game,s->script_object);
	}
	#endif
}
int finishScriptBlock(script_manager *sm,script_block *backup,lua_State *lua){
	void *game;
	
	#ifndef DSL_SERVER_VERSION
	if(game = sm->dsl->game){
		if(sm->running_script)
			removeGameScriptObject(game,sm->running_script->script_object);
		if(backup->s)
			addGameScriptObject(game,backup->s->script_object);
		else if(backup->so)
			setGameScriptObject(game,backup->so);
		else
			setGameScriptIndex(game,-1);
	}
	#endif
	return finishExecution(sm,backup->c,backup->s,backup->t,lua,backup->b);
}

// Script manager update.
static void updateThread(script_manager *sm,script *s,thread *t,lua_Debug *ar,lua_State *lua){
	lua_State *co;
	int status;
	
	co = t->coroutine;
	#ifndef DSL_SERVER_VERSION
	#ifdef USE_FAKE_THREADS
	setScriptObjectVirtualThread(s->script_object,co);
	#endif
	#endif
	runScriptEvent(sm->dsl->events,lua,EVENT_THREAD_UPDATE,t);
	if(t->flags & PASS_ARGS_TO_THREAD){
		t->flags &= ~PASS_ARGS_TO_THREAD;
		status = lua_resume(co,lua_gettop(co) - 1);
	}else
		status = lua_resume(co,0);
	if(status){
		t->flags |= ERROR_RUNNING_THREAD;
		if(lua_isstring(co,-1))
			printConsoleError(sm->dsl->console,"%s%s",getDslPrintPrefix(sm->dsl,1),lua_tostring(co,-1));
		else
			printConsoleError(sm->dsl->console,"%s%s",getDslPrintPrefix(sm->dsl,1),TEXT_FAIL_UNKNOWN);
		shutdownThread(t,lua,1);
	}else if(!lua_getstack(co,0,ar))
		shutdownThread(t,lua,1);
	else
		lua_settop(co,0);
}
void updateScriptManagerInit(script_manager *sm,lua_State *lua){
	script_collection *cc,*nc;
	script *cs,*ns;
	thread *gamemain;
	lua_Debug ar;
	#ifndef DSL_SERVER_VERSION
	void *game;
	void *so;
	int i;
	#endif
	
	#ifndef DSL_SERVER_VERSION
	if((game = sm->dsl->game) && (i = getGameScriptIndex(game)) != -1)
		so = getGameScriptPool(game)[i];
	#endif
	sm->running_script_update = 1;
	for(cc = sm->collections;cc;cc = nc){
		startExecution(sm,cc,NULL,NULL);
		for(cs = cc->scripts;cs;cs = ns){
			startExecution(sm,cc,cs,NULL);
			#ifndef DSL_SERVER_VERSION
			if(game)
				addGameScriptObject(game,cs->script_object);
			#endif
			if(gamemain = activateThread(cs,lua,GAME_THREAD,0,"gamemain")){
				startExecution(sm,cc,cs,gamemain);
				updateThread(sm,cs,gamemain,&ar,lua);
				finishExecution(sm,cc,cs,NULL,lua,0);
			}
			#ifndef DSL_SERVER_VERSION
			if(game)
				removeGameScriptObject(game,cs->script_object);
			#endif
			ns = cs->next;
			finishExecution(sm,cc,NULL,NULL,lua,0);
		}
		nc = cc->next;
		finishExecution(sm,NULL,NULL,NULL,lua,0);
	}
	sm->running_script_update = 0;
	#ifndef DSL_SERVER_VERSION
	if(game)
		if(i == -1)
			setGameScriptIndex(game,-1);
		else
			setGameScriptObject(game,so);
	if((i = lua_getgcthreshold(lua)) < 0 || i > GC_MAX_THRESHOLD)
		lua_setgcthreshold(lua,0); // fix threshold
	#endif
}
void updateScriptManagerUpdate(script_manager *sm,lua_State *lua,int type){
	script_collection *cc,*nc;
	script *cs,*ns;
	thread *ct,*nt;
	lua_Debug ar;
	unsigned t;
	#ifndef DSL_SERVER_VERSION
	void *game;
	void *so;
	int i;
	#endif
	
	t = getGameTimer();
	#ifndef DSL_SERVER_VERSION
	if((game = sm->dsl->game) && (i = getGameScriptIndex(game)) != -1)
		so = getGameScriptPool(game)[i];
	#endif
	sm->running_script_update = 1;
	for(cc = sm->collections;cc;cc = nc){
		startExecution(sm,cc,NULL,NULL);
		for(cs = cc->scripts;cs;cs = ns){
			startExecution(sm,cc,cs,NULL);
			#ifndef DSL_SERVER_VERSION
			if(game)
				addGameScriptObject(game,cs->script_object);
			#endif
			for(ct = cs->threads[type];ct;ct = nt){
				startExecution(sm,cc,cs,ct);
				if(t >= ct->when)
					updateThread(sm,cs,ct,&ar,lua);
				nt = ct->next;
				finishExecution(sm,cc,cs,NULL,lua,0);
			}
			#ifndef DSL_SERVER_VERSION
			if(game)
				removeGameScriptObject(game,cs->script_object);
			#endif
			ns = cs->next;
			finishExecution(sm,cc,NULL,NULL,lua,0);
		}
		nc = cc->next;
		finishExecution(sm,NULL,NULL,NULL,lua,0);
	}
	sm->running_script_update = 0;
	#ifndef DSL_SERVER_VERSION
	if(game)
		if(i == -1)
			setGameScriptIndex(game,-1);
		else
			setGameScriptObject(game,so);
	if((i = lua_getgcthreshold(lua)) < 0 || i > GC_MAX_THRESHOLD)
		lua_setgcthreshold(lua,0); // fix threshold
	#endif
}