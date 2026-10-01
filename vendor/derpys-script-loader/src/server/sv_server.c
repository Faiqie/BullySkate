/* DERPY'S SCRIPT SERVER: NETWORKING
	
	server networking stuff
	
	lua events:
	PlayerListing: when a player needs listing info
	PlayerConnecting: when a player is FLAG_CONNECTED
	PlayerConnected: when a player is FLAG_PLAYING
	PlayerDropped: when a player that was FLAG_CONNECTED is no longer
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h> // struct addrinfo
#include <fcntl.h> // fcntl / related
#include <arpa/inet.h> // inet_ntop
#endif

// POSIX PORT
#ifndef _WIN32
#define WSAAddressToString(sa,length,whatever,address,addrsize) (inet_ntop(AF_INET,&((struct sockaddr_in*)sa)->sin_addr,address,*addrsize) ? 0 : SOCKET_ERROR)
#endif

// PLAYER FLAGS
#define FLAG_VALID 1 // an invalid player means that slot is free (and that the whole player struct is zero'd)
#define FLAG_KICKED 2 // close the socket as soon as the send buffer is empty and don't recv more
#define FLAG_TIMEOUT 4 // the player timeout is active and they'll be dropped if they go silent
#define FLAG_FULLSEND 8 // the send buffer was full so the player should be dropped
#define FLAG_NOMESSAGE 16 // the player has't sent any messages yet
#define FLAG_CONNECTED 32 // connected (not necessarily playing but never on when kicked)
#define FLAG_NEEDFILES 64 // need to start a file update
#define FLAG_RETRYFILE 128 // need to retry file update (because a file was busy)
#define FLAG_PLAYING 256 // is fully connected and playing (can still be considered playing even if they need more files)
#define FLAG_SENDHEART 512 // should send heartbeat messages
#define FLAG_KICKTIMER 1024 // don't close the socket immediately but wait for the timeout
#define FLAG_DELAYKICK 2048 // only turn on FLAG_SHOULDKICK instead of kicking
#define FLAG_SHOULDKICK 4096

// DISCONNECT REASONS
#define REASON_NONE 0
#define REASON_TIMEOUT 1
#define REASON_BUFFER 2
#define REASON_KICKED 3

// MISC DEFINES
#define EXTRA_PLAYER_COUNT 16 // extra slots on top of the real max_players so listing players can still be added
#define FILE_UPDATE_BUFFER 8192
#define FILE_NEED_RESPONSE 524288 // 512 KB (how many bytes it takes to need to wait for a response, should be a bit less than NET_MAX_MESSAGE_BUFFER)
#define KICK_TIME_BUFFER 8000 // for FLAG_KICKTIMER

// NETWORK TYPES
typedef struct file_update{
	loader_collection *lc; // the collection currently being worked on
	unsigned index; // progress in the collection
	FILE *file;
	int stage;
	int sent; // amount of messages sent (that need to be responded to before another update can occur)
}file_update;

/* UTILITY */

// GENERAL UTILITY
static int setSocketNonBlocking(SOCKET s){
	#ifdef _WIN32
	u_long value;
	#else
	int fl;
	#endif
	
	#ifdef _WIN32
	value = 1;
	return !ioctlsocket(s,FIONBIO,&value) && value;
	#else
	return (fl = fcntl(s,F_GETFL)) >= 0 && fcntl(s,F_SETFL,fl|O_NONBLOCK) != -1;
	#endif
}
static int checkStringChars(const char *str){
	char c;
	
	for(;c = *str;str++)
		if(!isprint(c))
			return c;
	return 0; // zero = safe
}
static int sendRejection(SOCKET s,int msg,const char *str){
	char *buffer;
	int size;
	
	// designed to just send a simple message before closing a socket
	buffer = malloc(size = strlen(str) + sizeof(net_msg_size) + 1);
	if(!buffer){
		#ifdef _WIN32
		WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
		#else
		errno = ENOMEM;
		#endif
		return SOCKET_ERROR;
	}
	*(net_msg_size*)buffer = size;
	buffer[sizeof(net_msg_size)] = msg;
	memcpy(buffer+sizeof(net_msg_size)+1,str,size-sizeof(net_msg_size)-1);
	size = send(s,buffer,size,0);
	free(buffer);
	return size;
}
static int removePort(char *ip){
	#ifdef _WIN32
	for(;*ip;ip++)
		if(*ip == ':')
			return *ip = 0;
	return 1;
	#else
	return 0; // inet_ntop doesn't include a port
	#endif
}

// PLAYER UTILITY
static char* getPlayerName(network_player *player){
	return *player->name && strcmp(player->name,"player") ? player->name : player->address;
}
static unsigned getPlayerId(network_state *net,network_player *player){
	unsigned id;
	
	for(id = 0;id < net->max_players;id++)
		if(net->players + id == player)
			return id;
	return -1;
	//return (player - net->players) / sizeof(network_player);
}
static unsigned getPlayerCount(network_state *net){
	network_player *player;
	unsigned count;
	
	count = 0;
	for(player = net->players;player < net->invalid_player;player++)
		if(player->flags & FLAG_PLAYING)
			count++;
	return count;
}
static void runDropEvent(lua_State *lua,dsl_state *dsl,unsigned id){
	if(!lua)
		lua = dsl->lua;
	lua_pushnumber(lua,id);
	runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"PlayerDropped",1);
	lua_pop(lua,1);
}
static void sendBytes(network_player *player,const void *buffer,int bytes){
	if(player->sndi + bytes <= NET_MAX_MESSAGE_BUFFER){
		memcpy(player->send+player->sndi,buffer,bytes);
		player->sndi += bytes;
	}else
		player->flags |= FLAG_FULLSEND;
}
static void kickPlayer(dsl_state *dsl,network_player *player,const char *reason,lua_State *lua){
	net_msg_size bytes;
	unsigned char msg;
	
	if(player->flags & FLAG_DELAYKICK){ // turned on during PlayerConnecting and PlayerConnected events
		strncpy(player->shouldkick,reason,NET_CLIENT_KICK_BYTES);
		player->shouldkick[NET_CLIENT_KICK_BYTES-1] = 0;
		player->flags |= FLAG_SHOULDKICK;
		return;
	}
	bytes = sizeof(net_msg_size) + 1 + strlen(reason);
	sendBytes(player,&bytes,sizeof(net_msg_size));
	msg = NET_SVM_FUCK_YOU;
	sendBytes(player,&msg,1);
	if(*reason)
		sendBytes(player,reason,strlen(reason));
	player->flags |= FLAG_KICKED | FLAG_KICKTIMER;
	player->flags &= ~FLAG_PLAYING;
	if(player->flags & FLAG_CONNECTED){
		if(*reason)
			printConsoleOutput(dsl->console,"{%s} kicked from the server: %s",getPlayerName(player),reason);
		else
			printConsoleOutput(dsl->console,"{%s} kicked from the server",getPlayerName(player));
		runDropEvent(lua,dsl,getPlayerId(dsl->network,player));
	}
	player->flags &= ~FLAG_CONNECTED;
	player->interacted = dsl->last_frame; // so that the kicked player has a moment before they are closed (just to make sure they get the kick message)
}

// PLAYER API
network_player* getNetworkPlayerById(network_state *net,unsigned id){
	network_player *result;
	
	if(id < net->max_players){
		result = net->players + id;
		if(result->flags & FLAG_CONNECTED)
			return result;
	}
	return NULL;
}
int isNetworkPlayerConnected(network_player *player,int playing){
	return player->flags & (playing ? FLAG_PLAYING : FLAG_CONNECTED);
}
void kickNetworkPlayer(lua_State *lua,dsl_state *dsl,network_player *player){
	if(~player->flags & FLAG_KICKED)
		kickPlayer(dsl,player,lua_tostring(lua,-1),lua);
	lua_pop(lua,1);
}
void sendNetworkPlayerEvent(network_player *player,const char *name,const void *buffer,int size,int opt){
	net_msg_size bytes;
	char msg;
	
	bytes = sizeof(net_msg_size) + 2 + strlen(name) + size;
	sendBytes(player,&bytes,sizeof(net_msg_size));
	msg = size && opt ? NET_SVM_SCRIPT_EVENTS2 : NET_SVM_SCRIPT_EVENTS;
	sendBytes(player,&msg,1);
	sendBytes(player,name,strlen(name)+1);
	if(size)
		sendBytes(player,buffer,size);
}

/* FILES */

// FILE STATE
static int startPlayerFiles(dsl_state *dsl,network_player *player){
	file_update *files;
	
	if(files = malloc(sizeof(file_update))){
		files->lc = NULL;
		files->file = NULL;
		files->sent = 0;
		player->files = files;
		debugNetworkMessage(dsl->console,"{%s} starting file update",getPlayerName(player));
		return 1;
	}
	printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"file update");
	kickPlayer(dsl,player,"The server failed to send you files.",NULL);
	return 0;
}
static void cleanPlayerFiles(dsl_state *dsl,network_player *player){
	file_update *files;
	
	if(files = player->files){
		player->files = NULL;
		if(files->file)
			fclose(files->file);
		free(files);
	}
}
int areNetworkFilesTransfering(network_state *net){
	network_player *player;
	
	for(player = net->players;player < net->invalid_player;player++)
		if(player->files)
			return 1;
	return 0;
}

// FILE UPDATE
static int fuShouldUseCollection(loader_collection *lc){
	return lc->sc && ~lc->flags & LOADER_STOPPING && lc->flags & LOADER_CLIENTFILES;
}
static int fuOpenCollection(dsl_state *dsl,file_update *files,unsigned id){
	loader_collection *lc;
	
	for(lc = dsl->loader->first;lc;lc = lc->next)
		if(fuShouldUseCollection(lc) && getScriptLoaderPlayerBit(lc,id)){
			files->lc = lc;
			break;
		}
	if(!files->lc)
		return 0;
	files->index = 0;
	files->stage = 0;
	return 1; // we got more work to do!
}
static const char* fuOpenFile(file_update *files){
	char buffer[sizeof(DSL_CLIENT_SCRIPTS_PATH)+MAX_LOADER_PATH-1];
	loader_collection *lc;
	const char *name;
	
	lc = files->lc;
	if(!fuShouldUseCollection(lc))
		return (void*)1; // we're done with this collection (but we didn't fail to open the file)
	if(files->stage == 2){
		if(!lc->sc_cfg || files->index)
			return (void*)1; // we're all done (either because no config or we already did it)
		name = DSL_SCRIPT_CONFIG; // assumed be less than MAX_LOADER_PATH
	}else{
		name = getConfigStringArray(lc->sc_cfg,files->stage ? "client_file" : "client_script",files->index);
		if(!name)
			return (void*)1; // no file was opened but not because of a failure
	}
	strcpy(buffer,DSL_CLIENT_SCRIPTS_PATH); // we know there's enough space because of copyClientFile in loader.c
	strcat(buffer,(char*)(lc+1));
	strcat(buffer,"/");
	strcat(buffer,name);
	if(files->file = fopen(buffer,"rb")){
		files->index++;
		return name;
	}
	return NULL;
}
static int fuSendPlaying(dsl_state *dsl,network_player *player,char *buffer){
	loader_collection *lc;
	net_msg_size bytes;
	size_t length;
	char scratch;
	char *name;
	
	length = 0;
	for(lc = dsl->loader->first;lc;lc = lc->next)
		if(fuShouldUseCollection(lc)){
			name = (char*)(lc+1);
			if(length + strlen(name) >= FILE_UPDATE_BUFFER)
				return 1;
			while(*name)
				buffer[length++] = *name++;
			buffer[length++] = 0;
		}
	bytes = sizeof(net_msg_size) + 1 + length;
	sendBytes(player,&bytes,sizeof(net_msg_size));
	scratch = NET_SVM_START_PLAYING;
	sendBytes(player,&scratch,1);
	sendBytes(player,buffer,length);
	return 0;
}
static void updatePlayerFiles(dsl_state *dsl,network_player *player){
	loader_collection *lc;
	char buffer[FILE_UPDATE_BUFFER];
	file_update *files;
	const char *name;
	net_msg_size bytes;
	char scratch;
	
	/*
		NET_SVM_START_FILE 3 // name of file
		NET_SVM_APPEND_FILE 4 // some amount of file data to append to the file currently being built
		NET_SVM_FINISH_FILE 5 // nothing else encoded, just tells client to close the file
	*/
	
	files = player->files;
	while(files->lc || fuOpenCollection(dsl,files,getPlayerId(dsl->network,player))){
		lc = files->lc;
		while(files->stage <= 2){
			if(!files->file){
				name = fuOpenFile(files);
				if(!name){
					player->flags |= FLAG_RETRYFILE; // try to update files again in player update
					return;
				}
				if(!files->file){
					files->index = 0;
					files->stage++;
					continue;
				}
				bytes = sizeof(net_msg_size) + 2 + strlen((char*)(lc+1)) + strlen(name);
				sendBytes(player,&bytes,sizeof(net_msg_size));
				*buffer = NET_SVM_START_FILE;
				sendBytes(player,buffer,1);
				sendBytes(player,(char*)(lc+1),strlen((char*)(lc+1)));
				*buffer = '/';
				sendBytes(player,buffer,1);
				sendBytes(player,name,strlen(name));
				debugNetworkMessage(dsl->console,"{%s} started sending \"%s\"",getPlayerName(player),name);
				files->sent++;
				if(player->sndi >= FILE_NEED_RESPONSE)
					return; // success! but let's wait for a response.
			}
			if(!feof(files->file)){
				bytes = fread(buffer,1,FILE_UPDATE_BUFFER,files->file);
				if(ferror(files->file)){
					kickPlayer(dsl,player,"The server failed to send you a file.",NULL);
					cleanPlayerFiles(dsl,player);
					return;
				}
				if(bytes){
					bytes += sizeof(net_msg_size) + 1;
					sendBytes(player,&bytes,sizeof(net_msg_size));
					scratch = NET_SVM_APPEND_FILE;
					sendBytes(player,&scratch,1);
					sendBytes(player,buffer,bytes-sizeof(net_msg_size)-1);
					debugNetworkMessage(dsl->console,"{%s} sending %zu bytes for file",getPlayerName(player),bytes-sizeof(net_msg_size)-1);
					files->sent++;
					if(player->sndi >= FILE_NEED_RESPONSE)
						return; // success! but let's wait for a response.
				}
			}
			if(feof(files->file)){
				bytes = sizeof(net_msg_size) + 1;
				sendBytes(player,&bytes,sizeof(net_msg_size));
				*buffer = NET_SVM_FINISH_FILE;
				sendBytes(player,buffer,1);
				fclose(files->file);
				files->file = NULL; // so the next one is opened next time
				debugNetworkMessage(dsl->console,"{%s} finished sending file",getPlayerName(player));
				files->sent++;
				if(player->sndi >= FILE_NEED_RESPONSE)
					return; // success! but let's wait for a response.
			}
		}
		bytes = sizeof(net_msg_size) + 1 + strlen((char*)(lc+1));
		sendBytes(player,&bytes,sizeof(net_msg_size));
		*buffer = NET_SVM_ADD_RESTART;
		sendBytes(player,buffer,1);
		sendBytes(player,(char*)(lc+1),strlen((char*)(lc+1)));
		setScriptLoaderPlayerBit(lc,getPlayerId(dsl->network,player),0); // done with this collection!
		files->lc = NULL;
		if(player->sndi >= FILE_NEED_RESPONSE)
			return; // success! but let's wait for a response.
	}
	cleanPlayerFiles(dsl,player);
	debugNetworkMessage(dsl->console,"{%s} done sending files",getPlayerName(player));
	if(fuSendPlaying(dsl,player,buffer))
		kickPlayer(dsl,player,"The server failed to send you starting information.",NULL);
}

/* STATE */

// PLAYER STATE
static network_player* createPlayer(dsl_state *dsl,network_state *net,SOCKET s,char *address,int *duplicate){
	network_player *player;
	
	for(player = net->players;player < net->invalid_player;player++)
		if(player->flags & FLAG_VALID && !strcmp(player->address,address)){
			*duplicate = 1;
			return NULL;
		}
	for(player = net->players;player < net->invalid_player;player++)
		if(~player->flags & FLAG_VALID)
			break;
	if(player == net->invalid_player){
		*duplicate = 0;
		return NULL;
	}
	player->flags |= FLAG_VALID | FLAG_TIMEOUT | FLAG_NOMESSAGE;
	player->client = s;
	player->interacted = dsl->last_frame;
	player->files = NULL;
	strcpy(player->address,address);
	return player;
}
static void destroyPlayer(dsl_state *dsl,network_player *player,int reason){
	player->flags &= ~FLAG_PLAYING;
	if(dsl && player->flags & FLAG_CONNECTED){
		if(reason == REASON_TIMEOUT)
			printConsoleOutput(dsl->console,"{%s} disconnected from the server (timed out)",getPlayerName(player));
		else if(reason == REASON_BUFFER)
			printConsoleOutput(dsl->console,"{%s} disconnected from the server (send buffer exausted)",getPlayerName(player));
		else if(reason == REASON_KICKED)
			printConsoleOutput(dsl->console,"{%s} disconnected from the server (kicked)",getPlayerName(player));
		else
			printConsoleOutput(dsl->console,"{%s} disconnected from the server",getPlayerName(player));
		runDropEvent(NULL,dsl,getPlayerId(dsl->network,player));
	}
	closesocket(player->client);
	cleanPlayerFiles(dsl,player);
	memset(player,0,sizeof(network_player));
	setScriptLoaderPlayerBits(dsl->loader,getPlayerId(dsl->network,player),1);
}

// SERVER STATE
static int setupIcon(dsl_state *dsl,network_state *net,const char *name){
	FILE *file;
	long bytes;
	
	file = fopen(name,"rb");
	if(!file){
		printConsoleError(dsl->console,"failed to load icon (%s: %s)",strerror(errno),name);
		return 1;
	}
	bytes = -1L;
	if(fseek(file,0,SEEK_END) || (bytes = ftell(file)) == -1L || bytes > NET_MAX_ICON_BYTES){
		if(bytes == -1L)
			printConsoleError(dsl->console,"unknown icon file size");
		else
			printConsoleError(dsl->console,"icon is too large (%lu / %u KB)",bytes/1024,NET_MAX_ICON_BYTES/1024);
		fclose(file);
		return 1;
	}
	net->iconsize = bytes;
	if(fseek(file,0,SEEK_SET) || !fread(net->icon,bytes,1,file)){
		printConsoleError(dsl->console,"failed to read icon");
		fclose(file);
		return 1;
	}
	fclose(file);
	return 0;
}
static int setupBasic(dsl_state *dsl,char *result,const char *key,const char *def,size_t length){
	const char *buffer;
	
	buffer = getConfigString(dsl->config,key);
	if(!buffer){
		if(!def)
			return 0;
		buffer = def;
	}
	if(strlen(buffer) > length){
		printConsoleError(dsl->console,"%s is too long (%zu / %d characters)",key,strlen(buffer),length);
		return 1;
	}
	strcpy(result,buffer);
	return 0;
}
static int setupBasics(dsl_state *dsl,network_state *net){
	const char *icon;
	int count;
	
	if(setupBasic(dsl,net->name,"server_name","Unconfigured Server",NET_MAX_NAME_LENGTH))
		return 1;
	if(setupBasic(dsl,net->info,"server_info","Come play!",NET_MAX_INFO_LENGTH))
		return 1;
	icon = getConfigString(dsl->config,"server_icon");
	if(icon && setupIcon(dsl,net,icon))
		return 1;
	if(setupBasic(dsl,net->ip,"server_ip","",NET_MAX_IP_LENGTH))
		return 1;
	if(setupBasic(dsl,net->port,"server_port",NET_DEFAULT_PORT,NET_MAX_PORT_LENGTH))
		return 1;
	count = getConfigInteger(dsl->config,"server_players");
	if(count < 1 || count > NET_MAX_PLAYER_COUNT){
		printConsoleError(dsl->console,"server_players is too high (%d / %d players)",count,NET_MAX_PLAYER_COUNT);
		return 1;
	}
	if(net->players = calloc(count+EXTRA_PLAYER_COUNT,sizeof(network_player))){
		net->invalid_player = net->players + count + EXTRA_PLAYER_COUNT;
		net->max_players = count;
		return 0;
	}
	printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"network players");
	return 1;
}
static int startListening(dsl_state *dsl,network_state *net){
	struct addrinfo hints,*list,*ai;
	
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if(getaddrinfo(*net->ip ? net->ip : NULL,net->port,&hints,&list)){
		#ifdef _WIN32
		printConsoleError(dsl->console,"bad host address (ip: %s, port: %s, wsa: %d)",net->ip,net->port,WSAGetLastError());
		#else
		printConsoleError(dsl->console,"bad host address (ip: %s, port: %s, reason: %s)",net->ip,net->port,strerror(errno));
		#endif
		return 1;
	}
	for(ai = list;ai;ai = ai->ai_next)
		if(ai->ai_family == hints.ai_family && ai->ai_socktype == hints.ai_socktype && ai->ai_protocol == hints.ai_protocol)
			break;
	if(!ai){
		printConsoleError(dsl->console,"no acceptable host address");
		freeaddrinfo(list);
		return 1;
	}
	net->listen = socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
	if(net->listen == INVALID_SOCKET){
		#ifdef _WIN32
		printConsoleError(dsl->console,"failed to create listening socket (wsa: %d)",WSAGetLastError());
		#else
		printConsoleError(dsl->console,"failed to create listening socket (reason: %s)",strerror(errno));
		#endif
		freeaddrinfo(list);
		return 1;
	}
	if(bind(net->listen,ai->ai_addr,ai->ai_addrlen)){
		#ifdef _WIN32
		printConsoleError(dsl->console,"failed to bind listening socket (wsa: %d)",WSAGetLastError());
		#else
		printConsoleError(dsl->console,"failed to bind listening socket (reason: %s)",strerror(errno));
		#endif
		freeaddrinfo(list);
		return 1;
	}
	freeaddrinfo(list);
	if(listen(net->listen,SOMAXCONN)){
		#ifdef _WIN32
		printConsoleError(dsl->console,"failed to listen for players (wsa: %d)",WSAGetLastError());
		#else
		printConsoleError(dsl->console,"failed to listen for players (reason: %s)",strerror(errno));
		#endif
		return 1;
	}
	if(setSocketNonBlocking(net->listen))
		return 0;
	#ifdef _WIN32
	printConsoleError(dsl->console,"failed to set non-blocking socket mode (wsa: %d)",WSAGetLastError());
	#else
	printConsoleError(dsl->console,"failed to set non-blocking socket mode (reason: %s)",strerror(errno));
	#endif
	return 1;
}
network_state *setupNetworking(dsl_state *dsl){
	network_state *net;
	#ifdef _WIN32
	WSADATA data;
	#endif
	int status;
	
	net = malloc(sizeof(network_state));
	if(!net){
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"network state");
		return NULL;
	}
	net->iconsize = 0;
	net->players = NULL;
	net->listen = INVALID_SOCKET;
	#ifdef _WIN32
	if(status = WSAStartup(2,&data)){
		printConsoleError(dsl->console,"failed to setup winsock2 (wsa: %d)",status);
		free(net);
		return NULL;
	}else if(data.wVersion & 0xFF != 2){
		printConsoleError(dsl->console,"bad winsock version (2 is required)");
		cleanupNetworking(dsl,net);
		return NULL;
	}else
	#endif
	if(setupBasics(dsl,net) || startListening(dsl,net)){
		cleanupNetworking(dsl,net);
		return NULL;
	}
	return net;
}
void cleanupNetworking(dsl_state *dsl,network_state *net){
	if(net->listen != INVALID_SOCKET)
		closesocket(net->listen);
	#ifdef _WIN32
	WSACleanup();
	#endif
	if(net->players){
		while(net->max_players)
			if(net->players[--net->max_players].flags & FLAG_VALID)
				destroyPlayer(NULL,net->players+net->max_players,REASON_NONE);
		free(net->players);
	}
	free(net);
}

/* MAIN */

// PROCESS MESSAGES
static int msgListing(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	char buffer[NET_MAX_INFO_LENGTH+1];
	char mode[NET_MAX_MODE_LENGTH+1];
	network_state *net;
	const char *info;
	lua_State *lua;
	char value[4];
	
	lua = dsl->lua;
	net = dsl->network;
	// lua event for listing
	lua_pushnumber(lua,getPlayerId(net,player));
	lua_newtable(lua);
	lua_pushstring(lua,"info");
	lua_pushstring(lua,net->info);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"mode");
	lua_pushstring(lua,"");
	lua_rawset(lua,-3);
	player->flags |= FLAG_CONNECTED; // temporarily considered connected so scripts can use player functions
	runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"PlayerListing",2);
	player->flags &= ~FLAG_CONNECTED;
	lua_pushstring(lua,"info");
	lua_rawget(lua,-2);
	if(lua_isstring(lua,-1)){
		info = strncpy(buffer,lua_tostring(lua,-1),NET_MAX_INFO_LENGTH);
		buffer[NET_MAX_INFO_LENGTH] = 0;
	}else
		info = net->info;
	lua_pushstring(lua,"mode");
	lua_rawget(lua,-3);
	if(lua_isstring(lua,-1)){
		strncpy(mode,lua_tostring(lua,-1),NET_MAX_MODE_LENGTH);
		mode[NET_MAX_MODE_LENGTH] = 0;
	}else
		*mode = 0;
	lua_pop(lua,4);
	// null terminated name, null terminated info, null terminated game mode, icon bytes, icon, players, max players
	*(net_msg_size*)value = sizeof(net_msg_size) + 1 + strlen(net->name) + 1 + strlen(info) + 1 + strlen(mode) + 1 + sizeof(uint32_t) + net->iconsize + sizeof(uint16_t) + sizeof(uint16_t);
	sendBytes(player,value,sizeof(net_msg_size));
	*(unsigned char*)value = NET_SVM_LIST_SERVER;
	sendBytes(player,value,1);
	sendBytes(player,net->name,strlen(net->name)+1);
	sendBytes(player,info,strlen(info)+1);
	sendBytes(player,mode,strlen(mode)+1);
	*(uint32_t*)value = net->iconsize;
	sendBytes(player,value,sizeof(uint32_t));
	if(net->iconsize)
		sendBytes(player,net->icon,net->iconsize);
	*(uint16_t*)value = getPlayerCount(net);
	sendBytes(player,value,sizeof(uint16_t));
	*(uint16_t*)value = net->max_players;
	sendBytes(player,value,sizeof(uint16_t));
	player->flags |= FLAG_KICKED; // so the socket closes once the listing is sent
	printConsoleOutput(dsl->console,"{%s} provided listing info",getPlayerName(player));
	return 0;
}
static int msgConnect(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	static const char *fields[CONTENT_TYPES] = {"act","cuts","trigger","ide","scripts","world"};
	network_state *net;
	lua_State *lua;
	int *hashes;
	int count;
	int index;
	
	printConsoleOutput(dsl->console,"{%s} connecting to the server",getPlayerName(player));
	lua = dsl->lua;
	net = dsl->network;
	player->flags |= FLAG_CONNECTED; // connected means they will start initialization, but are considered "connecting" to scripts
	hashes = (int*)(message + sizeof(uint32_t) + sizeof(NET_SIGNATURE));
	bytes -= sizeof(uint32_t) + sizeof(NET_SIGNATURE);
	if(*(unsigned*)hashes % sizeof(int) || (count = *(unsigned*)hashes / sizeof(int)) < CONTENT_TYPES + 1){ // +1 since the count includes itself
		sprintf(dsl->network->error,"corrupted hashes (%d)",count);
		return 1;
	}
	lua_pushnumber(lua,getPlayerId(net,player));
	lua_newtable(lua);
	for(index = CONTENT_TYPES + 1;index < count;index++){
		lua_pushlightuserdata(lua,(void*)hashes[index]);
		lua_rawseti(lua,-2,index-CONTENT_TYPES);
	}
	index = 0;
	while(index < CONTENT_TYPES){
		lua_pushstring(lua,fields[index++]);
		lua_pushlightuserdata(lua,(void*)hashes[index]);
		lua_rawset(lua,-3);
	}
	player->flags |= FLAG_DELAYKICK;
	runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"PlayerConnecting",2);
	player->flags &= ~FLAG_DELAYKICK;
	if(player->flags & FLAG_SHOULDKICK)
		kickPlayer(dsl,player,player->shouldkick,lua);
	lua_pop(lua,2);
	if(~player->flags & FLAG_KICKED)
		player->flags |= FLAG_NEEDFILES;
	return 0;
}
static int msgAlright(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	file_update *files;
	
	if(files = player->files){
		if(!files->sent){
			strcpy(dsl->network->error,"unexpected file update response");
			return 1;
		}
		if(!--files->sent)
			updatePlayerFiles(dsl,player);
	}
	return 0;
}
static int msgImready(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	lua_State *lua;
	
	if(player->flags & FLAG_PLAYING){
		strcpy(dsl->network->error,"unexpected ready message");
		return 1;
	}
	printConsoleOutput(dsl->console,"{%s} connected to the server",getPlayerName(player));
	lua = dsl->lua;
	player->flags |= FLAG_PLAYING | FLAG_SENDHEART; // playing means they have finished initialization, but are considered "connected" to scripts
	lua_pushnumber(lua,getPlayerId(dsl->network,player));
	player->flags |= FLAG_DELAYKICK;
	runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"PlayerConnected",1);
	player->flags &= ~FLAG_DELAYKICK;
	if(player->flags & FLAG_SHOULDKICK)
		kickPlayer(dsl,player,player->shouldkick,lua);
	lua_pop(lua,1);
	return 0;
}
static int msgSevents(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	network_state *net;
	lua_State *lua;
	char *name;
	int args;
	int i;
	
	net = dsl->network;
	if(~player->flags & FLAG_PLAYING){
		strcpy(net->error,"unexpected network script event");
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
	lua_pushnumber(lua,getPlayerId(net,player));
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
	args++; // + 1 for player
	if(args > NET_ARG_LIMIT)
		lua_pop(lua,args-NET_ARG_LIMIT);
	runLuaScriptEvent(dsl->events,lua,REMOTE_EVENT,name,args);
	lua_pop(lua,args);
	return 0;
}
static int msgImalive(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	if(~player->flags & FLAG_PLAYING){
		strcpy(dsl->network->error,"unexpected heartbeat");
		return 1;
	}
	player->flags |= FLAG_SENDHEART;
	return 0;
}
static int msgSevents2(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	network_state *net;
	lua_State *lua;
	char *name;
	
	net = dsl->network;
	if(~player->flags & FLAG_PLAYING){
		strcpy(net->error,"unexpected network script event 2");
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
	lua_pushnumber(lua,getPlayerId(net,player));
	if(!unpackLuaTable(lua,message,bytes)){
		strcpy(net->error,lua_tostring(lua,-1));
		lua_pop(lua,2);
		return 1;
	}
	runLuaScriptEvent(dsl->events,lua,REMOTE_EVENT,name,2);
	lua_pop(lua,2);
	return 0;
}
static int initialRequest(network_player *player,net_msg_size bytes,char *message){
	// dsl version, net sig, hashes, username
	if(bytes < sizeof(uint32_t) + sizeof(NET_SIGNATURE) + sizeof(unsigned))
		return 1;
	if(*(uint32_t*)message != DSL_VERSION)
		return 1;
	message += sizeof(uint32_t);
	if(strcmp(message,NET_SIGNATURE))
		return 1;
	message += sizeof(NET_SIGNATURE);
	bytes -= sizeof(uint32_t) + sizeof(NET_SIGNATURE);
	if(bytes < *(unsigned*)message)
		return 1;
	bytes -= *(unsigned*)message;
	message += *(unsigned*)message;
	if(bytes >= NET_MAX_USERNAME_BYTES)
		bytes = NET_MAX_USERNAME_BYTES - 1;
	while(bytes && isspace(*message)){
		message++; // skipping whitespace at start
		bytes--;
	}
	if(bytes){
		message = strncpy(player->name,message,bytes); // we know it's null terminated because the entire player was zero'd
		while(isspace(message[--bytes]))
			message[bytes] = 0; // trimming whitespace at end
	}
	if(checkStringChars(player->name))
		return 2;
	return 0;
}
static int callMessage(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	bytes--;
	switch(*message++){
		case NET_MSG_LISTING:
			debugNetworkMessage(dsl->console,"{%s} NET_MSG_LISTING",getPlayerName(player));
			return msgListing(dsl,player,bytes,message);
		case NET_MSG_CONNECT:
			debugNetworkMessage(dsl->console,"{%s} NET_MSG_CONNECT",getPlayerName(player));
			return msgConnect(dsl,player,bytes,message);
		case NET_MSG_ALRIGHT:
			debugNetworkMessage(dsl->console,"{%s} NET_MSG_ALRIGHT",getPlayerName(player));
			return msgAlright(dsl,player,bytes,message);
		case NET_MSG_IMREADY:
			debugNetworkMessage(dsl->console,"{%s} NET_MSG_IMREADY",getPlayerName(player));
			return msgImready(dsl,player,bytes,message);
		case NET_MSG_SEVENTS:
			debugNetworkMessageEx(dsl->console,"{%s} NET_MSG_SEVENTS",getPlayerName(player));
			return msgSevents(dsl,player,bytes,message);
		case NET_MSG_IMALIVE:
			debugNetworkMessageEx(dsl->console,"{%s} NET_MSG_IMALIVE",getPlayerName(player));
			return msgImalive(dsl,player,bytes,message);
		case NET_MSG_SEVENTS2:
			debugNetworkMessageEx(dsl->console,"{%s} NET_MSG_SEVENTS2",getPlayerName(player));
			return msgSevents2(dsl,player,bytes,message);
	}
	sprintf(dsl->network->error,"unknown server message (%hhu)",*(message-1));
	return 1;
}
static int processMessage(dsl_state *dsl,network_player *player,net_msg_size bytes,char *message){
	int status;
	
	if(player->flags & FLAG_KICKED)
		return 0;
	player->interacted = dsl->last_frame;
	if(player->flags & FLAG_NOMESSAGE){
		player->flags &= ~FLAG_NOMESSAGE;
		if(*message != NET_MSG_LISTING && *message != NET_MSG_CONNECT)
			return 1; // client should have sent one of these first
		if(status = initialRequest(player,bytes-1,message+1)){
			if(status == 1){
				sprintf(dsl->network->error,"Your version is not supported, please switch to %s.",DSL_VERSION_TEXT);
				kickPlayer(dsl,player,dsl->network->error,NULL);
			}else
				kickPlayer(dsl,player,"Your username contains unacceptable characters, please fix it.",NULL);
			return 0;
		}
	}else if(~player->flags & FLAG_CONNECTED)
		return 1; // client isn't connected after first message so fuck off
	bytes = callMessage(dsl,player,bytes,message);
	#ifdef NET_PRINT_DEBUG_MESSAGES
	if(bytes == 1)
		printConsoleWarning(dsl->console,"{%s} failed to process message: %s",getPlayerName(player),dsl->network->error);
	#endif
	return bytes;
}

// UPDATE PLAYER
static void sendHeartbeat(network_player *player){
	net_msg_size bytes;
	char msg;
	
	bytes = sizeof(net_msg_size) + 1;
	sendBytes(player,&bytes,sizeof(net_msg_size));
	msg = NET_SVM_ARE_YOU_ALIVE;
	sendBytes(player,&msg,1);
}
static void updatePlayer(dsl_state *dsl,network_player *player){
	if(!player->files && player->flags & FLAG_NEEDFILES){
		player->flags &= ~FLAG_NEEDFILES;
		cleanPlayerFiles(dsl,player);
		if(startPlayerFiles(dsl,player))
			updatePlayerFiles(dsl,player);
	}else if(player->flags & FLAG_RETRYFILE){
		player->flags &= ~FLAG_RETRYFILE;
		if(player->files && !player->files->sent) // if already waiting for a response on something then we'll just wait for that
			updatePlayerFiles(dsl,player);
	}
	if(player->flags & FLAG_SENDHEART && dsl->last_frame - player->interacted >= NET_HEARTBEAT){
		player->flags &= ~FLAG_SENDHEART;
		sendHeartbeat(player);
	}
}
void updateNetworkPlayerFiles(dsl_state *dsl){
	network_player *player;
	network_state *net;
	
	net = dsl->network;
	for(player = net->players;player < net->invalid_player;player++)
		if(player->flags & FLAG_VALID)
			player->flags |= FLAG_NEEDFILES;
}

// UPDATE SERVER
static int shouldAcceptClient(dsl_state *dsl,network_state *net,const char *address){
	config_file *cfg;
	const char *str;
	int array;
	
	cfg = dsl->config;
	if(getConfigBoolean(cfg,"whitelist_instead")){
		for(array = 0;str = getConfigStringArray(cfg,"whitelist_ip",array);array++)
			if(!strcmp(address,str))
				return 0;
		return 1;
	}
	for(array = 0;str = getConfigStringArray(cfg,"blacklist_ip",array);array++)
		if(!strcmp(address,str))
			return 2;
	return 0;
}
static void acceptClients(dsl_state *dsl,network_state *net){ // TODO
	char address[NET_CLIENT_ADDR_BYTES];
	network_player *player;
	struct sockaddr sa;
	DWORD addrsize;
	int length;
	int status;
	SOCKET s;
	
	while(length = sizeof(struct sockaddr),(s = accept(net->listen,&sa,&length)) != INVALID_SOCKET){
		if(!setSocketNonBlocking(s)){
			printConsoleError(dsl->console,"{?} failed to set non-blocking socket mode");
			closesocket(s);
			continue;
		}
		addrsize = NET_CLIENT_ADDR_BYTES;
		// TODO: remove the player if they're connected for listing (right now they'll just get told they're already in this server)
		if(WSAAddressToString(&sa,length,NULL,address,&addrsize) || removePort(address)){
			#ifdef NET_PRINT_DEBUG_MESSAGES
			#ifdef _WIN32
			if((status = WSAGetLastError()) == WSAEFAULT)
				printConsoleWarning(dsl->console,"{?} closed socket with oversized ip address (%d / %d bytes)",addrsize,NET_CLIENT_ADDR_BYTES);
			else
				printConsoleWarning(dsl->console,"{?} closed socket because of unknown ip address (wsa: %d)",status);
			#else
			printConsoleWarning(dsl->console,"{?} closed socket because of bad ip address");
			#endif
			#endif
			sendRejection(s,NET_SVM_FUCK_YOU,"Your IP was considered invalid by the server.");
			closesocket(s);
		}else if(status = shouldAcceptClient(dsl,net,address)){
			#ifdef NET_PRINT_DEBUG_MESSAGES
			if(status == 1)
				printConsoleOutput(dsl->console,"rejected %s because they aren't on the whitelist",address);
			else
				printConsoleOutput(dsl->console,"rejected %s because they are on the blacklist",address);
			#endif
			sendRejection(s,NET_SVM_FUCK_YOU,"You are not allowed on this server.");
			closesocket(s);
		}else if(player = createPlayer(dsl,net,s,address,&status)){
			#ifdef NET_PRINT_DEBUG_MESSAGES
			printConsoleOutput(dsl->console,"{%s} accepted player",address);
			#endif
			*(net_msg_size*)address = sizeof(net_msg_size) + 1; // reusing this buffer to supply data to sendBytes
			sendBytes(player,address,sizeof(net_msg_size));
			*(unsigned char*)address = NET_SVM_WHATS_UP;
			sendBytes(player,address,1);
		}else if(status){
			#ifdef NET_PRINT_DEBUG_MESSAGES
			printConsoleWarning(dsl->console,"{%s} rejected because their ip is already here",address);
			#endif
			sendRejection(s,NET_SVM_FUCK_YOU,"You are already in this server!");
			closesocket(s);
		}else{
			#ifdef NET_PRINT_DEBUG_MESSAGES
			printConsoleOutput(dsl->console,"{%s} rejected because the server is full",address);
			#endif
			sendRejection(s,NET_SVM_FUCK_YOU,"Sorry, the server is full!");
			closesocket(s);
		}
	}
	#ifdef _WIN32
	if((status = WSAGetLastError()) != WSAECONNRESET && status != WSAEWOULDBLOCK)
		printConsoleWarning(dsl->console,"error occurred while accepting players (wsa: %d)",status);
	#else
	// should catch EINTR?
	if(errno != ECONNABORTED && errno != EAGAIN && errno != EWOULDBLOCK)
		printConsoleWarning(dsl->console,"error occurred while accepting players (reason: %s)",strerror(errno));
	#endif
}
void updateNetworking(dsl_state *dsl,network_state *net){
	network_player *player;
	
	acceptClients(dsl,net);
	for(player = net->players;player < net->invalid_player;player++)
		if(player->flags & FLAG_VALID)
			if(recvNetworkMessages(dsl,player->client,getPlayerName(player),player->recv,&player->rcvi,&processMessage,player)){
				destroyPlayer(dsl,player,REASON_NONE);
			}else if(player->flags & FLAG_TIMEOUT && dsl->last_frame - player->interacted >= NET_TIMEOUT){
				#ifdef NET_PRINT_DEBUG_MESSAGES
				printConsoleOutput(dsl->console,"{%s} player timed out",getPlayerName(player));
				#endif
				destroyPlayer(dsl,player,REASON_TIMEOUT);
			}else if(player->flags & FLAG_CONNECTED)
				updatePlayer(dsl,player);
}
void updateNetworking2(dsl_state *dsl,network_state *net){
	network_player *player;
	
	for(player = net->players;player < net->invalid_player;player++)
		if(player->flags & FLAG_VALID)
			if(player->flags & FLAG_FULLSEND){
				#ifdef NET_PRINT_DEBUG_MESSAGES
				printConsoleWarning(dsl->console,"{%s} send buffer space was exausted",getPlayerName(player));
				#endif
				destroyPlayer(dsl,player,REASON_BUFFER);
			}else if(sendNetworkMessages(dsl,player->client,getPlayerName(player),player->send,&player->sndi)){
				destroyPlayer(dsl,player,REASON_NONE);
			}else if(!player->sndi && player->flags & FLAG_KICKED && (~player->flags & FLAG_KICKTIMER || dsl->last_frame - player->interacted >= KICK_TIME_BUFFER))
				destroyPlayer(dsl,player,REASON_KICKED);
}