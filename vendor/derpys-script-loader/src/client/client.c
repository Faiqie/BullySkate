/* DERPY'S SCRIPT LOADER: NETWORKING
	
	client networking stuff
	
	general events:
	ServerListing(address,listing) : got server listing info
	ServerConnecting(address)      : start connecting to server to play
	
	connected events (after ServerConnecting):
	ServerConnected()    : started playing on server
	ServerDisconnected() : disconnected from server
	ServerDownloading()  : downloading files for server (just once before Connected)
	ServerKicked(reason) : kicked from server (a disconnected will still come seperately)
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ws2tcpip.h>

// NETWORK FLAGS
#define FLAG_SETUPWSA 1
#define FLAG_PROMPTING 2
#define FLAG_PROMPTED 4
#define FLAG_PTRUSTED 8 // current prompt is trusted for listing
#define FLAG_PLOADING 16 // current prompt is loading listing information
#define FLAG_PLAYING 32 // a server is going to be used for play (so only allow one)

// SERVER FLAGS
#define FLAG_TIMEOUT 1 // enforce timeout
#define FLAG_LISTING 2 // listing instead of playing server
#define FLAG_CONNECTED 4 // the socket is valid and connected (doesn't mean we're "playing" on the server, we might just be listing)
#define FLAG_FULLSEND 8 // close server because send buffer is full
#define FLAG_PLISTING 16 // listing information goes to prompt
#define FLAG_NOCONMSG 32 // don't show the connect / disconnect messages
#define FLAG_DOWNLOAD 64 // started file downloading
#define FLAG_STARTED 128 // started playing

// MISCELLANEOUS
#define MAX_SERVERS 12
#define MAX_LUA_LISTING 8
#define MAX_RESTART_BYTES 32768 // 32 KB
#define PROMPT_TIME_REQUIREMENT 500
#define SERVER_ADDR_BYTES 64

// NETWORK TYPES
typedef struct server_listing{
	char name[NET_MAX_NAME_LENGTH+1];
	char info[NET_MAX_INFO_LENGTH+1];
	char mode[NET_MAX_MODE_LENGTH+1];
	int icon; // lua reference
	render_texture *texture; // only valid while the lua object is valid
	unsigned players;
	unsigned max_players;
}server_listing;
typedef struct server_state{
	int flags;
	FILE *file; // downloading files
	SOCKET server; // server is only valid if this is
	unsigned interacted; // for timeout
	char address[SERVER_ADDR_BYTES];
	server_listing *listing;
	struct sockaddr *addr;
	int addrlen;
	char send[NET_MAX_MESSAGE_BUFFER];
	int sndi;
	char recv[NET_MAX_MESSAGE_BUFFER];
	int rcvi;
}server_state;
typedef struct network_scripts{
	unsigned count;
	char **active;
	// all the actual strings follow this struct
}network_scripts;
struct network_state{
	int flags;
	char ip[NET_MAX_HOST_BYTES]; // server we might connect to (valid when FLAG_PROMPTING or FLAG_PLAYING)
	char port[NET_MAX_PORT_LENGTH+1];
	unsigned prompting; // when we started prompting
	render_font *pfontg; // george do be bold
	render_font *pfonta; // arial
	server_listing *plisting;
	network_scripts *starting;
	char restarts[MAX_RESTART_BYTES];
	unsigned restart_size;
	char error[NET_MAX_ERROR_BUFFER]; // stores errors from some deeper functions so a single print call can be written later
	server_state servers[MAX_SERVERS]; // only one server if FLAG_PLAYING otherwise these are just for listing
};

/* UTILITY */

// LISTING
static void destroyListing(dsl_state *dsl,server_listing *listing){
	if(dsl && listing->icon != LUA_NOREF)
		luaL_unref(dsl->lua,LUA_REGISTRYINDEX,listing->icon);
	free(listing);
}
static int createListingTexture2(lua_State *lua){
	createLuaTexture2(lua,lua_touserdata(lua,1),"server_icon",DSL_SERVER_ICON_FILE);
	return 1;
}
static int createListingTexture(dsl_state *dsl,server_listing *listing,char *data,size_t iconbytes){
	const char *error;
	lua_State *lua;
	FILE *icon;
	
	if(!checkDslPathExists(DSL_SERVER_ICON_FILE,1)){
		strcpy(dsl->network->error,"failed to create cache for server icon");
		return 1;
	}
	icon = fopen(DSL_SERVER_ICON_FILE,"wb");
	if(!icon){
		sprintf(dsl->network->error,"failed to create server icon file (%s: %s)",strerror(errno),DSL_SERVER_ICON_FILE);
		return 1;
	}
	if(!fwrite(data,iconbytes,1,icon)){
		strcpy(dsl->network->error,"failed to write server icon file");
		fclose(icon);
		return 1;
	}
	fclose(icon);
	lua = dsl->lua;
	lua_pushcfunction(lua,&createListingTexture2);
	lua_pushlightuserdata(lua,dsl->render);
	if(lua_pcall(lua,1,1,0)){
		error = lua_tostring(lua,-1);
		sprintf(dsl->network->error,"failed to create server icon texture (%s)",error ? error : TEXT_FAIL_UNKNOWN);
		lua_pop(lua,1);
		return 1;
	}
	listing->texture = ((lua_texture*)lua_touserdata(lua,-1))->texture;
	listing->icon = luaL_ref(lua,LUA_REGISTRYINDEX);
	return 0;
}

// OTHER
static int* getHashes(script_manager *sm,net_msg_size *result){
	int *hashes;
	int count;
	script_collection *c;
	script *s;
	
	count = CONTENT_TYPES + 1; // +1 for count
	for(c = sm->collections;c;c = c->next)
		for(s = c->scripts;s;s = s->next)
			if(s->flags & HASHED_SCRIPT)
				count++;
	hashes = malloc(*result = count * sizeof(int));
	if(!hashes)
		return NULL;
	*(unsigned*)hashes = count * sizeof(int);
	for(count = 0;count < CONTENT_TYPES;count++)
		hashes[count+1] = getContentHash(count);
	count = CONTENT_TYPES + 1;
	for(c = sm->collections;c;c = c->next)
		for(s = c->scripts;s;s = s->next)
			if(s->flags & HASHED_SCRIPT)
				hashes[count++] = s->hash;
	return hashes;
}
static void freeNetworkScripts(network_scripts *ns){
	if(ns->active)
		free(ns->active);
	free(ns);
}

/* MESSAGES */

// SEND
static void sendBytes(server_state *svs,const void *buffer,int bytes){
	if(svs->sndi + bytes <= NET_MAX_MESSAGE_BUFFER){
		memcpy(svs->send+svs->sndi,buffer,bytes);
		svs->sndi += bytes;
	}else
		svs->flags |= FLAG_FULLSEND;
}
static void sendBasicMessage(server_state *svs,char msg){
	net_msg_size bytes;
	
	bytes = sizeof(net_msg_size) + 1;
	sendBytes(svs,&bytes,sizeof(net_msg_size));
	sendBytes(svs,&msg,1);
}
int sendNetworkServerEvent(network_state *net,const char *name,const void *buffer,int size,int opt){
	net_msg_size bytes;
	server_state *svs;
	char msg;
	
	for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
		if(svs->server != INVALID_SOCKET && ~svs->flags & FLAG_LISTING)
			break;
	if(!svs || ~svs->flags & FLAG_STARTED)
		return 1;
	bytes = sizeof(net_msg_size) + 2 + strlen(name) + size;
	sendBytes(svs,&bytes,sizeof(net_msg_size));
	msg = size && opt ? NET_MSG_SEVENTS2 : NET_MSG_SEVENTS;
	sendBytes(svs,&msg,1);
	sendBytes(svs,name,strlen(name)+1);
	if(size)
		sendBytes(svs,buffer,size);
	return 0;
}

// READING
static int getMsgString(network_state *net,net_msg_size *bytes,char **message,char *buffer,size_t length){
	net_msg_size index;
	char *str;
	
	str = *message;
	for(index = 0;index < *bytes;index++)
		if(!str[index]){
			if(buffer){
				if(index > length){ // length arg does not include null terminator
					strcpy(net->error,"oversized string in message");
					return 1;
				}
				strcpy(buffer,*message);
			}
			index++;
			*bytes -= index;
			*message += index;
			return 0;
		}
	strcpy(net->error,"corrupted string in message");
	return 1; // not a null-terminated string
}
static int useMsgBytes(network_state *net,net_msg_size *bytes,net_msg_size needed){
	if(*bytes < needed){
		strcpy(net->error,"invalid message size");
		return 1; // not enough space
	}
	*bytes -= needed;
	return 0;
}

// TYPES
static int msgKicked(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	lua_State *lua;
	int index;
	
	if(svs->flags & FLAG_LISTING)
		return 2; // don't show kick stuff if we were just listing
	lua = dsl->lua;
	lua_pushlstring(lua,message,bytes);
	runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"ServerKicked",1);
	lua_pop(lua,1);
	if(bytes)
		printConsoleOutput(dsl->console,"{%s} kicked from server: %.*s",svs->address,bytes,message);
	else
		printConsoleOutput(dsl->console,"{%s} kicked from server",svs->address);
	#ifndef NET_PRINT_DEBUG_MESSAGES
	svs->flags |= FLAG_NOCONMSG; // because we already showed being kicked
	#endif
	return 2; // close server but not an error
}
static int msgWhatup(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	char buffer[NET_MAX_USERNAME_BYTES];
	const char *username;
	net_msg_size hash_bytes;
	int *hashes;
	char value[4];
	
	username = getConfigString(dsl->config,"username");
	if(!username)
		username = "player";
	if(strlen(username) >= NET_MAX_USERNAME_BYTES){
		username = strncpy(buffer,username,NET_MAX_USERNAME_BYTES-1);
		buffer[NET_MAX_USERNAME_BYTES-1] = 0;
	}
	if(svs->flags & FLAG_LISTING){
		hashes = NULL;
		hash_bytes = sizeof(int);
	}else if(!(hashes = getHashes(dsl->manager,&hash_bytes))){
		sprintf(dsl->network->error,TEXT_FAIL_ALLOCATE,"script hashes");
		return 1;
	}
	*(net_msg_size*)value = sizeof(net_msg_size) + 1 + sizeof(uint32_t) + sizeof(NET_SIGNATURE) + hash_bytes + strlen(username);
	sendBytes(svs,value,sizeof(net_msg_size));
	*value = hashes ? NET_MSG_CONNECT : NET_MSG_LISTING;
	sendBytes(svs,value,1);
	*(uint32_t*)value = DSL_VERSION;
	sendBytes(svs,value,sizeof(uint32_t));
	sendBytes(svs,NET_SIGNATURE,sizeof(NET_SIGNATURE));
	if(hashes){
		sendBytes(svs,hashes,hash_bytes);
		free(hashes);
	}else{
		*(int*)value = sizeof(int);
		sendBytes(svs,value,sizeof(int));
	}
	if(*username)
		sendBytes(svs,username,strlen(username));
	return 0;
}
static int msgListing(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	server_listing *listing;
	network_state *net;
	size_t iconbytes;
	lua_State *lua;
	
	net = dsl->network;
	listing = malloc(sizeof(server_listing));
	if(!listing){
		sprintf(net->error,TEXT_FAIL_ALLOCATE,"server listing");
		return 1;
	}
	svs->listing = listing;
	// null terminated name, null terminated info, null terminated game mode, icon bytes, icon, players, max players
	if(getMsgString(net,&bytes,&message,listing->name,NET_MAX_NAME_LENGTH))
		return 1;
	if(getMsgString(net,&bytes,&message,listing->info,NET_MAX_INFO_LENGTH))
		return 1;
	if(getMsgString(net,&bytes,&message,listing->mode,NET_MAX_MODE_LENGTH))
		return 1;
	if(useMsgBytes(net,&bytes,sizeof(uint32_t)))
		return 1;
	iconbytes = *((uint32_t*)message)++;
	if(iconbytes){
		if(useMsgBytes(net,&bytes,iconbytes))
			return 1;
		if(createListingTexture(dsl,listing,message,iconbytes))
			return 1;
		message += iconbytes;
	}else
		listing->icon = LUA_NOREF;
	if(useMsgBytes(net,&bytes,4))
		return 1;
	listing->players = *((uint16_t*)message)++;
	listing->max_players = *(uint16_t*)message;
	lua = dsl->lua;
	lua_pushstring(lua,svs->address);
	lua_newtable(lua);
	lua_pushstring(lua,"name");
	lua_pushstring(lua,listing->name);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"info");
	lua_pushstring(lua,listing->info);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"mode");
	lua_pushstring(lua,listing->mode);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"icon");
	lua_rawgeti(lua,LUA_REGISTRYINDEX,listing->icon);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"players");
	lua_pushnumber(lua,listing->players);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"max_players");
	lua_pushnumber(lua,listing->max_players);
	lua_rawset(lua,-3);
	runLuaScriptEvent(dsl->events,dsl->lua,LOCAL_EVENT,"ServerListing",2);
	lua_pop(lua,2);
	if(svs->flags & FLAG_PLISTING){
		if(net->plisting)
			destroyListing(dsl,net->plisting);
		net->plisting = svs->listing;
		svs->listing = NULL;
	}
	return 2; // close the server but don't not as an error
}
static int msgStartFile(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	char buffer[sizeof(DSL_SERVER_SCRIPTS_PATH)+MAX_LOADER_PATH-1];
	network_state *net;
	
	net = dsl->network;
	if(svs->file || svs->flags & FLAG_LISTING){
		strcpy(net->error,"file download was unexpected");
		return 1;
	}
	if(~svs->flags & FLAG_DOWNLOAD){
		runLuaScriptEvent(dsl->events,dsl->lua,LOCAL_EVENT,"ServerDownloading",0);
		printConsoleOutput(dsl->console,"{%s} downloading client files",svs->address);
		svs->flags |= FLAG_DOWNLOAD;
	}
	if(bytes >= MAX_LOADER_PATH){
		strcpy(net->error,"path of client file is too big");
		return 1;
	}
	strcpy(buffer,DSL_SERVER_SCRIPTS_PATH);
	strncat(buffer,message,bytes);
	if(!checkNetworkExtension(buffer) || !isDslFileSafe(dsl,buffer)){
		sprintf(net->error,"attempted to download unsafe file (%s)",buffer);
		return 1;
	}
	if(!checkDslPathExists(buffer,1)){
		strcpy(net->error,"failed to create server cache folders");
		return 1;
	}
	svs->file = fopen(buffer,"wb");
	if(!svs->file){
		sprintf(net->error,"failed to create server file (%s: %s)",strerror(errno),buffer);
		return 1;
	}
	sendBasicMessage(svs,NET_MSG_ALRIGHT);
	return 0;
}
static int msgAppendFile(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	if(!svs->file){
		strcpy(dsl->network->error,"file append was unexpected");
		return 1;
	}
	if(bytes && fwrite(message,bytes,1,svs->file)){
		sendBasicMessage(svs,NET_MSG_ALRIGHT);
		return 0;
	}
	strcpy(dsl->network->error,"failed to write file being downloaded");
	return 1;
}
static int msgFinishFile(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	if(!svs->file){
		strcpy(dsl->network->error,"file finish was unexpected");
		return 1;
	}
	fclose(svs->file);
	svs->file = NULL;
	sendBasicMessage(svs,NET_MSG_ALRIGHT);
	return 0;
}
static int msgAddRestart(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	network_state *net;
	
	net = dsl->network;
	if(svs->flags & FLAG_LISTING){
		strcpy(net->error,"restart message was unexpected");
		return 1;
	}
	if(net->restart_size + bytes >= MAX_RESTART_BYTES){
		strcpy(net->error,"network script collection limit reached");
		return 1;
	}
	strncpy(net->restarts+net->restart_size,message,bytes);
	net->restarts[net->restart_size += bytes] = 0;
	net->restart_size++;
	return 0;
}
static int msgStartPlaying(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	network_scripts *ns;
	network_state *net;
	net_msg_size index;
	
	net = dsl->network;
	if(svs->flags & FLAG_LISTING){
		strcpy(net->error,"play message was unexpected");
		return 1;
	}
	if(net->starting){
		freeNetworkScripts(net->starting);
		net->starting = NULL;
	}
	ns = malloc(sizeof(network_scripts) + bytes);
	if(!ns){
		sprintf(net->error,TEXT_FAIL_ALLOCATE,"network scripts info");
		return 1;
	}
	ns->count = 0;
	for(index = 0;index < bytes;index++)
		if(!message[index])
			ns->count++;
	if(ns->count){
		if(message[bytes-1]){
			strcpy(net->error,"unfinished string in message");
			free(ns);
			return 1;
		}
		ns->active = malloc(sizeof(char*) * ns->count);
		if(!ns->active){
			sprintf(net->error,TEXT_FAIL_ALLOCATE,"active network scripts");
			free(ns);
			return 1;
		}
		ns->active[0] = (char*)(ns+1);
		ns->count = 1;
		for(index = 0;index < bytes;)
			if(!message[index++] && index < bytes)
				ns->active[ns->count++] = (char*)(ns+1) + index;
		memcpy(ns+1,message,bytes);
	}else
		ns->active = NULL;
	net->starting = ns;
	if(~svs->flags & FLAG_STARTED)
		printConsoleOutput(dsl->console,"{%s} stopping local scripts",svs->address);
	prepareScriptLoaderForServer(dsl->loader); // will call preparedNetworkScripts
	return 0;
}
static int msgScriptEvents(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	network_state *net;
	lua_State *lua;
	char *name;
	int args;
	int i;
	
	net = dsl->network;
	if(~svs->flags & FLAG_STARTED){
		strcpy(net->error,"unexpected network script event");
		return 1;
	}
	if(!bytes){
		strcpy(net->error,"unexpected network script event message");
		return 1;
	}
	lua = dsl->lua;
	name = message;
	while(bytes){
		bytes--;
		if(!*message++)
			break;
		if(!bytes){
			strcpy(net->error,"corrupted network script event message");
			return 1;
		}
	}
	if(bytes){
		if(!unpackLuaTable(lua,message,bytes)){
			strcpy(net->error,lua_tostring(lua,-1));
			lua_pop(lua,1);
			return 1;
		}
		lua_pushstring(lua,"n");
		lua_rawget(lua,-2);
		args = lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : luaL_getn(lua,-2);
		lua_pop(lua,1);
		if(args < 0 || !lua_checkstack(lua,args+LUA_MINSTACK)){
			strcpy(net->error,"failed to run network event");
			return 1;
		}
		for(i = 1;i <= args;i++)
			lua_rawgeti(lua,-i,i);
		lua_remove(lua,-i);
	}else
		args = 0;
	if(args > NET_ARG_LIMIT)
		lua_pop(lua,args-NET_ARG_LIMIT);
	runLuaScriptEvent(dsl->events,lua,REMOTE_EVENT,name,args);
	lua_pop(lua,args);
	return 0;
}
static int msgAreYouAlive(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	if(svs->flags & FLAG_STARTED){
		sendBasicMessage(svs,NET_MSG_IMALIVE);
		return 0;
	}
	strcpy(dsl->network->error,"unexpected heartbeat message");
	return 1;
}
static int msgScriptEvents2(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	network_state *net;
	lua_State *lua;
	char *name;
	
	net = dsl->network;
	if(~svs->flags & FLAG_STARTED){
		strcpy(net->error,"unexpected network script event 2");
		return 1;
	}
	if(!bytes){
		strcpy(net->error,"unexpected network script event message 2");
		return 1;
	}
	lua = dsl->lua;
	name = message;
	while(bytes){
		bytes--;
		if(!*message++)
			break;
		if(!bytes){
			strcpy(net->error,"corrupted network script event message 2");
			return 1;
		}
	}
	if(!bytes){
		strcpy(net->error,"undersized network script event message 2");
		return 1;
	}
	if(!unpackLuaTable(lua,message,bytes)){
		strcpy(net->error,lua_tostring(lua,-1));
		lua_pop(lua,1);
		return 1;
	}
	runLuaScriptEvent(dsl->events,lua,REMOTE_EVENT,name,1);
	lua_pop(lua,1);
	return 0;
}

// PROCESS
static int callMessage(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	bytes--;
	switch(*message++){
		case NET_SVM_FUCK_YOU:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_FUCK_YOU",svs->address);
			return msgKicked(dsl,svs,bytes,message);
		case NET_SVM_WHATS_UP:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_WHATS_UP",svs->address);
			return msgWhatup(dsl,svs,bytes,message);
		case NET_SVM_LIST_SERVER:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_LIST_SERVER",svs->address);
			return msgListing(dsl,svs,bytes,message);
		case NET_SVM_START_FILE:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_START_FILE",svs->address);
			return msgStartFile(dsl,svs,bytes,message);
		case NET_SVM_APPEND_FILE:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_APPEND_FILE",svs->address);
			return msgAppendFile(dsl,svs,bytes,message);
		case NET_SVM_FINISH_FILE:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_FINISH_FILE",svs->address);
			return msgFinishFile(dsl,svs,bytes,message);
		case NET_SVM_ADD_RESTART:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_ADD_RESTART",svs->address);
			return msgAddRestart(dsl,svs,bytes,message);
		case NET_SVM_START_PLAYING:
			debugNetworkMessage(dsl->console,"{%s} NET_SVM_START_PLAYING",svs->address);
			return msgStartPlaying(dsl,svs,bytes,message);
		case NET_SVM_SCRIPT_EVENTS:
			debugNetworkMessageEx(dsl->console,"{%s} NET_SVM_SCRIPT_EVENTS",svs->address);
			return msgScriptEvents(dsl,svs,bytes,message);
		case NET_SVM_ARE_YOU_ALIVE:
			debugNetworkMessageEx(dsl->console,"{%s} NET_SVM_ARE_YOU_ALIVE",svs->address);
			return msgAreYouAlive(dsl,svs,bytes,message);
		case NET_SVM_SCRIPT_EVENTS2:
			debugNetworkMessageEx(dsl->console,"{%s} NET_SVM_SCRIPT_EVENTS2",svs->address);
			return msgScriptEvents2(dsl,svs,bytes,message);
	}
	sprintf(dsl->network->error,"unknown server message (%hhu)",*(message-1));
	return 1;
}
static int processMessage(dsl_state *dsl,server_state *svs,net_msg_size bytes,char *message){
	svs->interacted = getGameTimer();
	bytes = callMessage(dsl,svs,bytes,message);
	if(bytes == 1)
		printConsoleError(dsl->console,"{%s} failed to process message: %s",svs->address,dsl->network->error);
	return bytes;
}

// PREPARED
void preparedNetworkScripts(dsl_state *dsl){
	network_scripts *ns;
	network_state *net;
	server_state *svs;
	unsigned index;
	
	net = dsl->network;
	if(ns = net->starting){
		if(net->flags & FLAG_PLAYING){
			for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
				if(svs->server != INVALID_SOCKET && ~svs->flags & FLAG_LISTING){
					if(~svs->flags & FLAG_STARTED){
						svs->flags |= FLAG_STARTED;
						svs->flags &= ~FLAG_TIMEOUT;
						sendBasicMessage(svs,NET_MSG_IMREADY);
						printConsoleOutput(dsl->console,"{%s} starting network scripts",svs->address);
						runLuaScriptEvent(dsl->events,dsl->lua,LOCAL_EVENT,"ServerConnected",0);
					}
					break;
				}
			refreshScriptLoaderServerScripts(dsl->loader,ns->count,ns->active);
			for(index = 0;index < net->restart_size;index++){
				restartScriptLoaderServerScript(dsl->loader,ns->count,ns->active,net->restarts+index);
				index += strlen(net->restarts+index);
			}
			ensureScriptLoaderServerScripts(dsl->loader,ns->count,ns->active); // ensure the right scripts are running (also the only thing that stops network scripts)
			net->restart_size = 0;
		}
		freeNetworkScripts(ns);
		net->starting = NULL;
	}
}

/* CONNECTION */

// DISCONNECT FROM SERVER
static void disconnectFromServer(dsl_state *dsl,server_state *svs,int clean){
	network_state *net;
	
	net = dsl->network;
	if(~svs->flags & FLAG_LISTING){
		if(clean){
			runLuaScriptEvent(dsl->events,dsl->lua,LOCAL_EVENT,"ServerDisconnected",0);
			killScriptLoaderServerScripts(dsl->loader,NULL);
		}
		if(net->starting){
			freeNetworkScripts(net->starting);
			net->starting = NULL;
		}
		net->flags &= ~FLAG_PLAYING;
		net->restart_size = 0;
	}
	if(svs->flags & FLAG_PLISTING)
		net->flags &= ~FLAG_PLOADING;
	if(svs->listing)
		destroyListing(clean ? dsl : NULL,svs->listing);
	if(svs->file)
		fclose(svs->file);
	if(svs->addr)
		free(svs->addr);
	if(svs->server != INVALID_SOCKET){
		if(~svs->flags & FLAG_NOCONMSG)
			printConsoleOutput(dsl->console,"{%s} disconnected from server",svs->address);
		closesocket(svs->server);
	}
	memset(svs,0,sizeof(server_state));
	svs->server = INVALID_SOCKET;
}
int disconnectFromPlaying(dsl_state *dsl){
	network_state *net;
	server_state *svs;
	
	net = dsl->network;
	for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
		if(svs->server != INVALID_SOCKET && ~svs->flags & FLAG_LISTING){
			disconnectFromServer(dsl,svs,1);
			return 0;
		}
	return 1;
}

// CONNECT TO SERVER
static server_state* actuallyConnect2(dsl_state *dsl,server_state *svs,const char *ip,const char *port,int listing){
	struct addrinfo hints,*list,*ai;
	network_state *net;
	server_state *dup;
	u_long blocking;
	DWORD addrsize;
	int status;
	
	net = dsl->network;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if(getaddrinfo(ip,port,&hints,&list)){
		sprintf(net->error,"{%s:%s} bad server address (wsa: %d)",ip,port,WSAGetLastError());
		return NULL;
	}
	for(ai = list;ai;ai = ai->ai_next)
		if(ai->ai_family == hints.ai_family && ai->ai_socktype == hints.ai_socktype && ai->ai_protocol == hints.ai_protocol)
			break;
	if(!ai){
		sprintf(net->error,"{%s:%s} no acceptable server address",ip,port);
		freeaddrinfo(list);
		return NULL;
	}
	addrsize = SERVER_ADDR_BYTES;
	if(WSAAddressToString(ai->ai_addr,ai->ai_addrlen,NULL,svs->address,&addrsize)){
		sprintf(net->error,"{%s:%s} failed to get server address string",ip,port);
		freeaddrinfo(list);
		*svs->address = 0; // just in case it was changed in the call
		return NULL;
	}
	for(dup = net->servers;dup < net->servers + MAX_SERVERS;dup++)
		if(dup->server != INVALID_SOCKET && !strcmp(dup->address,svs->address)){
			sprintf(net->error,"{%s} already connected to server",svs->address);
			freeaddrinfo(list);
			return NULL;
		}
	if(svs->addr)
		free(svs->addr);
	svs->addr = malloc(svs->addrlen = ai->ai_addrlen);
	if(!svs->addr){
		sprintf(net->error,"{%s} "TEXT_FAIL_ALLOCATE,svs->address,"connection address");
		freeaddrinfo(list);
		return NULL;
	}
	memcpy(svs->addr,ai->ai_addr,ai->ai_addrlen);
	svs->server = socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
	if(svs->server == INVALID_SOCKET){
		sprintf(net->error,"{%s} failed to create server socket (wsa: %d)",svs->address,WSAGetLastError());
		freeaddrinfo(list);
		return NULL;
	}
	blocking = 1;
	if(ioctlsocket(svs->server,FIONBIO,&blocking) || !blocking){
		sprintf(net->error,"{%s} failed to setup non-blocking socket (wsa: %d)",svs->address,WSAGetLastError());
		closesocket(svs->server);
		svs->server = INVALID_SOCKET;
		freeaddrinfo(list);
		return NULL;
	}
	if(connect(svs->server,ai->ai_addr,ai->ai_addrlen) && (status = WSAGetLastError()) != WSAEWOULDBLOCK){
		sprintf(net->error,"{%s} failed to start connecting to server (wsa: %d)",svs->address,status);
		closesocket(svs->server);
		svs->server = INVALID_SOCKET;
		freeaddrinfo(list);
		return NULL;
	}
	freeaddrinfo(list);
	svs->flags |= FLAG_TIMEOUT; // timeout mode is enabled
	if(listing)
		svs->flags |= FLAG_LISTING;
	if(status != WSAEWOULDBLOCK)
		svs->flags |= FLAG_CONNECTED;
	svs->interacted = getGameTimer();
	return svs;
}
static server_state* actuallyConnect(dsl_state *dsl,network_state *net,const char *ip,const char *port,int listing){
	server_state *svs;
	WSADATA data;
	int status;
	
	if(~net->flags & FLAG_SETUPWSA){
		if(status = WSAStartup(2,&data)){
			sprintf(net->error,"{%s:%s} failed to setup winsock2 (wsa: %d)",ip,port,status);
			return NULL;
		}else if(data.wVersion & 0xFF != 2){
			sprintf(net->error,"{%s:%s} bad winsock version (2 is required)",ip,port);
			WSACleanup();
			return NULL;
		}
		net->flags |= FLAG_SETUPWSA;
	}
	if(listing){
		for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
			if(svs->server == INVALID_SOCKET){
				if(svs = actuallyConnect2(dsl,svs,ip,port,1)){
					#ifndef NET_PRINT_DEBUG_MESSAGES
					svs->flags |= FLAG_NOCONMSG;
					#endif
				}
				return svs;
			}
		sprintf(net->error,"{%s:%s} server limit reached",ip,port);
	}else if(~net->flags & FLAG_PLAYING){
		dsl->flags |= DSL_CONNECTED_TO_SERVER;
		for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
			if(svs->server != INVALID_SOCKET)
				disconnectFromServer(dsl,svs,1);
		if(svs = actuallyConnect2(dsl,net->servers,ip,port,0)){
			lua_pushstring(dsl->lua,svs->address);
			runLuaScriptEvent(dsl->events,dsl->lua,LOCAL_EVENT,"ServerConnecting",1);
			lua_pop(dsl->lua,1);
			if(svs->flags & FLAG_CONNECTED)
				printConsoleOutput(dsl->console,"{%s} connected to server immediately",svs->address);
			else
				printConsoleOutput(dsl->console,"{%s} connecting to server",svs->address);
			net->flags |= FLAG_PLAYING;
		}
		return svs;
	}else
		sprintf(net->error,"{%s:%s} already connected to another server",ip,port);
	return NULL;
}

// CONNECT FUNCTION
static char* seperatePort(char *ip){
	char *port;
	
	port = NULL;
	for(port = NULL;*ip;ip++)
		if(*ip == ':')
			port = ip;
	if(!port)
		return NET_DEFAULT_PORT;
	*port = 0;
	return port + 1;
}
static int canListServer(dsl_state *dsl,const char *ip,const char *port,int limit){
	char buffer[NET_MAX_HOST_BYTES+NET_MAX_PORT_LENGTH+1];
	network_state *net;
	server_state *svs;
	const char *sv;
	int array;
	
	strcpy(buffer,ip);
	strcat(buffer,":");
	strcat(buffer,port);
	if(limit){
		net = dsl->network;
		for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
			if(svs->server != INVALID_SOCKET && !--limit)
				return 0;
	}
	for(array = 0;sv = getConfigStringArray(dsl->config,"list_server",array);array++)
		if(!strcmp(sv,buffer))
			return 1;
	return 0;
}
int promptNetworkServer(dsl_state *dsl,char *host,int printerror){
	network_state *net;
	server_state *svs;
	char *port;
	
	port = seperatePort(host);
	if(strlen(host) >= NET_MAX_HOST_BYTES || strlen(port) > NET_MAX_PORT_LENGTH){
		if(printerror)
			printConsoleError(dsl->console,"invalid server address or port number");
		return 0;
	}
	net = dsl->network;
	if(net->flags & FLAG_PLAYING)
		for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
			if(svs->server != INVALID_SOCKET && svs->flags & FLAG_CONNECTED && ~svs->flags & FLAG_LISTING){
				if(printerror)
					printConsoleError(dsl->console,"already connected to %s:%s",net->ip,net->port);
				return 0;
			}
	if(net->flags & (FLAG_PROMPTING | FLAG_PLAYING)){
		if(printerror)
			printConsoleError(dsl->console,"already connecting to %s:%s",net->ip,net->port);
		return 0;
	}
	if(canListServer(dsl,host,port,MAX_SERVERS)){
		net->flags |= FLAG_PTRUSTED;
		if(svs = actuallyConnect(dsl,net,host,port,1)){
			net->flags |= FLAG_PLOADING;
			svs->flags |= FLAG_PLISTING;
		}else
			net->flags &= ~FLAG_PLOADING;
	}else
		net->flags &= ~(FLAG_PTRUSTED | FLAG_PLOADING);
	strcpy(net->ip,host);
	strcpy(net->port,port);
	net->flags |= FLAG_PROMPTING;
	net->prompting = getGameTimer();
	net->plisting = NULL;
	return 1;
}

// CONNECT COMMAND
static void serverConnect(dsl_state *dsl,int argc,char **argv){
	if(!argc)
		printConsoleError(dsl->console,"expected server address");
	else if(getGamePaused())
		printConsoleError(dsl->console,"cannot connect while game is paused");
	else
		promptNetworkServer(dsl,*argv,1);
}

// CONNECT PROMPT
typedef struct prompt_metrics{
	float x;
	float y;
	float width;
	float leftw; // width of the left content
	float keybw; // width of the connect key box relative to the connect text box
	float height;
	float iconw;
	float iconh;
	float padx; // total padding for boxes (distributed to both sides)
	float pady;
	float borx; // total padding for borders
	float bory;
	float tpadx; // total padding for title text
	float tpady;
	float spadx; // total padding for other strings
	float spady;
}prompt_metrics;
static void getPromptMetrics(dsl_state *dsl,prompt_metrics *pm){
	float ar;
	#ifdef RUN_LUA_PROMPT_EDITOR_EVENT
	lua_State *lua;
	#endif
	
	ar = getRenderAspect(dsl->render);
	pm->iconh = 0.165f;
	pm->pady = 0.007f;
	pm->bory = 0.0035f;
	pm->tpady = 0.007f;
	pm->spady = 0.01f;
	pm->iconw = pm->iconh / ar;
	pm->padx = pm->pady / ar;
	pm->borx = pm->bory / ar;
	pm->tpadx = pm->pady / ar;
	pm->spadx = pm->spady / ar;
	pm->width = 0.7f / ar;
	pm->leftw = pm->width - pm->iconw - pm->padx;
	pm->keybw = 0.25f;
	pm->x = 0.5f - pm->width * 0.5f;
	pm->y = 0.3f;
}
static int drawPrompt(dsl_state *dsl,network_state *net,render_state *render){
	char buffer[NET_MAX_HOST_BYTES+NET_MAX_PORT_LENGTH+1]; // must also be big enough for NET_MAX_MODE_LENGTH
	server_listing *listing;
	render_text_draw title;
	render_text_draw name;
	render_text_draw info;
	render_text_draw button;
	render_text_draw extra;
	render_text_draw ip;
	prompt_metrics pm;
	unsigned alpha;
	float x,y,w,h; // for drawing the buttons
	int i;
	
	listing = net->plisting;
	alpha = (getGameTimer() - net->prompting) / (PROMPT_TIME_REQUIREMENT / 255.0f);
	if(alpha > 255)
		alpha = 255;
	alpha <<= 24;
	getPromptMetrics(dsl,&pm);
	title.text = "CONNECTING TO SERVER";
	title.flags = RENDER_TEXT_CENTER | RENDER_TEXT_CLIP_WIDTH | RENDER_TEXT_VERTICALLY_CENTER | RENDER_TEXT_BOLD;
	title.color = alpha | 0x00C8C878;
	title.size = 0.03f;
	title.width = pm.width - pm.tpadx;
	title.x = pm.x + pm.width * 0.5f;
	title.y = pm.y + (title.size + pm.tpady) * 0.5f;
	if(listing){
		name.text = listing->name;
		info.text = listing->info;
	}else if(net->flags & FLAG_PLOADING){
		name.text = "Loading server information...";
		info.text = NULL;
	}else if(net->flags & FLAG_PTRUSTED){
		name.text = "Unknown Server";
		info.text = "An error occurred while getting server information.";
	}else{
		name.text = "Unlisted Server";
		info.text = "This server is not listed in your DSL config.";
	}
	name.flags = RENDER_TEXT_CLIP_WIDTH | RENDER_TEXT_BOLD;
	name.color = alpha | 0x00C0C0C0;
	name.size = 0.025f;
	name.width = pm.leftw - (pm.padx + pm.borx + pm.spadx);
	name.x = pm.x + (pm.padx + pm.borx + pm.spadx) * 0.5f;
	name.y = pm.y + title.size + pm.tpady + (pm.pady + pm.bory + pm.spady) * 0.5f;
	info.flags = RENDER_TEXT_WORD_WRAP | RENDER_TEXT_CLIP_HEIGHT;
	info.color = alpha | 0x00C0C0C0;
	info.size = 0.02f;
	info.width = name.width;
	info.height = 0.15f;
	info.x = name.x;
	info.y = name.y + name.size + pm.pady * 0.5f + pm.bory + pm.spady;
	button.text = "Connect";
	button.flags = RENDER_TEXT_CLIP_WIDTH;
	button.color = alpha | 0x00C0C0C0;
	button.size = 0.02f;
	button.width = pm.leftw * 0.35f - (pm.padx + pm.spadx);
	button.x = info.x - pm.borx * 0.5f;
	button.y = info.y + info.height + (pm.pady + pm.bory) * 0.5f + pm.spady;
	pm.height = (button.y + button.size + (pm.pady + pm.spady) * 0.5f) - pm.y;
	extra.text = buffer;
	extra.flags = RENDER_TEXT_RIGHT | RENDER_TEXT_CLIP_WIDTH | RENDER_TEXT_BOLD;
	extra.color = alpha | 0x00C0C0C0;
	extra.size = 0.015f;
	extra.width = pm.width - pm.leftw - pm.padx * 0.5f;
	extra.x = pm.x + pm.width - pm.padx * 0.5f;
	extra.y = pm.y + title.size + pm.tpady + pm.iconh + pm.pady * 1.5f;
	ip.text = buffer;
	ip.flags = RENDER_TEXT_RIGHT | RENDER_TEXT_CLIP_WIDTH | RENDER_TEXT_DRAW_TEXT_UPWARDS | RENDER_TEXT_BLACK;
	ip.color = alpha | 0x00404040;
	ip.size = 0.015f;
	ip.width = pm.width - pm.padx;
	ip.x = pm.x + pm.width - pm.padx * 0.5f;
	ip.y = pm.y + pm.height - pm.pady * 0.5f;
	if(!drawRenderRectangle(render,pm.x,pm.y,pm.width,pm.height,alpha|0x00101010)) // background
		return 1;
	if(!drawRenderRectangle(render,pm.x,pm.y,pm.width,title.size+pm.tpady,alpha|0x00000000)) // title bar
		return 1;
	if(!drawRenderRectangle(render,name.x-(pm.borx+pm.spadx)*0.5f,name.y-(pm.bory+pm.spady)*0.5f,name.width+pm.borx+pm.spadx,name.size+pm.bory+pm.spady,alpha|0x00202020)) // name border
		return 1;
	if(!drawRenderRectangle(render,name.x-pm.spadx*0.5f,name.y-pm.spady*0.5f,name.width+pm.spadx,name.size+pm.spady,alpha|0x00181818)) // name box
		return 1;
	if(!drawRenderRectangle(render,info.x-(pm.borx+pm.spadx)*0.5f,info.y-(pm.bory+pm.spady)*0.5f,info.width+pm.borx+pm.spadx,info.height+pm.bory+pm.spady,alpha|0x00202020)) // info border
		return 1;
	if(!drawRenderRectangle(render,info.x-pm.spadx*0.5f,info.y-pm.spady*0.5f,info.width+pm.spadx,info.height+pm.spady,alpha|0x00181818)) // info box
		return 1;
	sprintf(buffer,"%s:%s",net->ip,net->port);
	if(!drawRenderString(render,net->pfonta,&ip,NULL,NULL)) // draw ip before buttons
		return 1;
	x = button.x - pm.spadx * 0.5f;
	y = button.y - pm.spady * 0.5f;
	w = button.width + pm.spadx;
	h = button.size + pm.spady;
	for(i = 0;i < 2;i++){
		if(!drawRenderRectangle(render,x+(w+pm.padx*0.5f)*i,y,w,h,alpha|0x00202020)) // connect box
			return 1;
		if(!drawRenderRectangle(render,(x+(w+pm.padx*0.5f)*i+w-pm.borx*0.5f)-(w-pm.borx)*pm.keybw,y+pm.bory*0.5f,(w-pm.borx)*pm.keybw,h-pm.bory,alpha|0x00181818)) // connect key
			return 1;
	}
	if(!drawRenderRectangle(render,pm.x+pm.width-(pm.iconw+pm.padx),pm.y+title.size+pm.tpady+pm.pady*0.5f,pm.iconw+pm.borx,pm.iconh+pm.bory,alpha|0x00202020)) // icon border
		return 1;
	if(listing && listing->icon != LUA_NOREF){
		if(!drawRenderTexture(render,listing->texture,pm.x+pm.width-(pm.iconw+pm.padx)+pm.borx*0.5f,pm.y+title.size+pm.tpady+(pm.pady+pm.bory)*0.5f,pm.iconw,pm.iconh,alpha|0x00FFFFFF)) // icon
			return 1;
	}else if(!drawRenderRectangle(render,pm.x+pm.width-(pm.iconw+pm.padx)+pm.borx*0.5f,pm.y+title.size+pm.tpady+(pm.pady+pm.bory)*0.5f,pm.iconw,pm.iconh,alpha|0x00181818)) // no icon
		return 1;
	if(!drawRenderString(render,net->pfontg,&title,NULL,NULL))
		return 1;
	if(!drawRenderString(render,net->pfontg,&name,NULL,NULL))
		return 1;
	if(info.text && !drawRenderString(render,net->pfontg,&info,NULL,NULL))
		return 1;
	button.flags |= RENDER_TEXT_CENTER;
	button.x += (button.width - (w - pm.borx) * pm.keybw - pm.borx * 0.5f) * 0.5f; // center
	if(!drawRenderString(render,net->pfontg,&button,NULL,NULL))
		return 1;
	button.text = "Cancel";
	button.x += w + pm.padx * 0.5f;
	if(!drawRenderString(render,net->pfontg,&button,NULL,NULL))
		return 1;
	button.flags &= ~RENDER_TEXT_CENTER;
	button.x -= (button.width - (w - pm.borx) * pm.keybw - pm.borx * 0.5f) * 0.5f;
	button.y += button.size * 0.5f;
	for(i = 0;i < 2;i++){
		button.text = i ? "F2" : "F1";
		button.flags |= RENDER_TEXT_CENTER | RENDER_TEXT_VERTICALLY_CENTER | RENDER_TEXT_BOLD;
		button.color = alpha | (i ? 0x00C00000 : 0x0000C000);
		button.width = (w - pm.borx) * pm.keybw;
		button.x = x + (w + pm.padx * 0.5f) * i + w - pm.borx * 0.5f - button.width * 0.5f;
		if(!drawRenderString(render,net->pfontg,&button,NULL,NULL))
			return 1;
	}
	if(listing){
		if(*listing->mode){
			sprintf(buffer,"Gamemode: %s",listing->mode);
			if(!drawRenderString(render,net->pfonta,&extra,NULL,NULL))
				return 1;
			extra.y += extra.size + pm.pady * 0.5f;
		}
		sprintf(buffer,"Players: %u / %u",listing->players,listing->max_players);
		if(!drawRenderString(render,net->pfonta,&extra,NULL,NULL))
			return 1;
	}
	return 0;
}
static void updatePromptInput(lua_State *lua,dsl_state *dsl,int key){
	network_state *net;
	
	net = dsl->network;
	if(net->flags & FLAG_PROMPTING && net->flags & FLAG_PROMPTED && getGameTimer() - net->prompting >= PROMPT_TIME_REQUIREMENT){
		if(key == DIK_F1){
			net->flags &= ~FLAG_PROMPTING;
			if(!actuallyConnect(dsl,net,net->ip,net->port,0))
				printConsoleRaw(dsl->console,CONSOLE_ERROR,net->error);
		}else if(key == DIK_F2)
			net->flags &= ~FLAG_PROMPTING;
	}
}
void updateNetworkPrompt(dsl_state *dsl,network_state *net){
	if(getGamePaused())
		net->flags &= ~FLAG_PROMPTING;
	if(net->flags & FLAG_PROMPTING)
		if(drawPrompt(dsl,net,dsl->render)){
			printConsoleError(dsl->console,"failed to render network prompt: %s",getRenderErrorString(dsl->render));
			net->flags &= ~FLAG_PROMPTED;
		}else
			net->flags |= FLAG_PROMPTED;
	else if(net->flags & FLAG_PROMPTED)
		net->flags &= ~FLAG_PROMPTED;
}

// DISCONNECT COMMAND
static void serverDisconnect(dsl_state *dsl,int argc,char **argv){
	if(disconnectFromPlaying(dsl))
		printConsoleError(dsl->console,"not connected to any server");
}

/* MISCELLANEOUS */

// UTILITY
const char* requestNetworkListing(dsl_state *dsl,const char *ip,const char *port){
	network_state *net;
	server_state *svs;
	
	net = dsl->network;
	if(net && ~net->flags & FLAG_PLAYING && canListServer(dsl,ip,port,MAX_LUA_LISTING) && (svs = actuallyConnect(dsl,net,ip,port,1)))
		return svs->address;
	return 0;
}

// KEYBOARD
int shouldNetworkDisableKeys(network_state *net){
	return net->flags & (FLAG_PROMPTING | FLAG_PROMPTED);
}

// QUERY
int isNetworkPlayingOnServer(network_state *net){
	return net->flags & FLAG_PLAYING;
}
int getNetworkSendBufferBytes(network_state *net){
	server_state *svs;
	
	for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
		if(svs->server != INVALID_SOCKET && ~svs->flags & FLAG_LISTING)
			return svs->sndi;
	return -1;
}

// UPDATE
static int updateConnecting(dsl_state *dsl,server_state *svs){
	int status;
	
	status = 0;
	if(connect(svs->server,svs->addr,svs->addrlen) && (status = WSAGetLastError()) != WSAEALREADY && status != WSAEINVAL && status != WSAEWOULDBLOCK && status != WSAEISCONN){
		printConsoleError(dsl->console,"{%s} failed to connect to server (wsa: %d)",svs->address,status);
		disconnectFromServer(dsl,svs,1);
	}else if(!status || status == WSAEISCONN){
		if(~svs->flags & FLAG_NOCONMSG)
			printConsoleOutput(dsl->console,"{%s} connected to server",svs->address);
		svs->interacted = getGameTimer();
		return svs->flags |= FLAG_CONNECTED;
	}
	return 0;
}
void updateNetworking(dsl_state *dsl,network_state *net){
	server_state *svs;
	
	for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
		if(svs->server != INVALID_SOCKET)
			// connect, receive messages, and check for timeout
			if((svs->flags & FLAG_CONNECTED || updateConnecting(dsl,svs)) && recvNetworkMessages(dsl,svs->server,svs->address,svs->recv,&svs->rcvi,&processMessage,svs)){
				disconnectFromServer(dsl,svs,1);
			}else if(svs->flags & FLAG_TIMEOUT && getGameTimer() - svs->interacted >= NET_TIMEOUT){
				if(~svs->flags & FLAG_NOCONMSG)
					printConsoleOutput(dsl->console,"{%s} server timed out",svs->address);
				disconnectFromServer(dsl,svs,1);
			}
}
void updateNetworking2(dsl_state *dsl,network_state *net){
	server_state *svs;
	
	for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
		if(svs->server != INVALID_SOCKET && svs->flags & FLAG_CONNECTED)
			// send messages
			if(svs->flags & FLAG_FULLSEND){
				printConsoleError(dsl->console,"{%s} send buffer space was exhausted",svs->address);
				disconnectFromServer(dsl,svs,1);
			}else if(sendNetworkMessages(dsl,svs->server,svs->address,svs->send,&svs->sndi))
				disconnectFromServer(dsl,svs,1);
}

// STATE
network_state *setupNetworking(dsl_state *dsl){
	network_state *net;
	server_state *svs;
	
	net = malloc(sizeof(network_state));
	if(!net)
		return NULL;
	net->flags = 0;
	net->pfontg = createRenderFont(dsl->render,NULL,"Georgia");
	net->pfonta = createRenderFont(dsl->render,NULL,"Arial");
	if(!net->pfontg || !net->pfonta){
		if(net->pfontg)
			destroyRenderFont(dsl->render,net->pfontg);
		if(net->pfonta)
			destroyRenderFont(dsl->render,net->pfonta);
		free(net);
		return NULL;
	}
	net->plisting = NULL;
	net->starting = NULL;
	net->restart_size = 0;
	memset(net->servers,0,sizeof(server_state)*MAX_SERVERS);
	for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
		svs->server = INVALID_SOCKET;
	setScriptCommandEx(dsl->cmdlist,"connect",TEXT_HELP_CONNECT,&serverConnect,dsl,1);
	setScriptCommandEx(dsl->cmdlist,"disconnect",TEXT_HELP_DISCONNECT,&serverDisconnect,dsl,1);
	addScriptEventCallback(dsl->events,EVENT_KEY_DOWN_UPDATE,(script_event_cb)&updatePromptInput,dsl);
	return net;
}
void cleanupNetworking(dsl_state *dsl,network_state *net){
	server_state *svs;
	
	if(net->flags & FLAG_SETUPWSA){
		for(svs = net->servers;svs < net->servers + MAX_SERVERS;svs++)
			if(svs->server != INVALID_SOCKET)
				disconnectFromServer(dsl,svs,0);
		if(net->plisting)
			destroyListing(NULL,net->plisting);
		if(net->starting)
			freeNetworkScripts(net->starting);
		WSACleanup();
	}
	destroyRenderFont(dsl->render,net->pfontg);
	destroyRenderFont(dsl->render,net->pfonta);
	free(net);
}