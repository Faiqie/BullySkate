/* DERPY'S SCRIPT LOADER: NETWORKING
	
	shared header for both the client and server
	the client and server both have their own source files plus a shared one
	
	about network messages:
	there are send and recv buffers that hold messages
	the first net_msg_size bytes of a message is the size of the full message
	the next byte is what the message type is then the rest is the actual message
	
*/

#ifndef DSL_NETWORK_H
#define DSL_NETWORK_H

struct dsl_state;
struct file_update;

// DEFAULTS
#define NET_DEFAULT_PORT "17017"
#define NET_DEFAULT_HZ 30

// LIMITS
#define NET_MAX_MESSAGE_BUFFER 1048576 // 1 MB
#define NET_MAX_PLAYER_COUNT 1024
#define NET_MAX_NAME_LENGTH 50
#define NET_MAX_INFO_LENGTH 400
#define NET_MAX_MODE_LENGTH 20
#define NET_MAX_ICON_BYTES 102400 // 100 KB
#define NET_MAX_IP_LENGTH 15
#define NET_MAX_PORT_LENGTH 5
#define NET_MAX_USERNAME_BYTES 32
#define NET_MAX_ERROR_BUFFER 1024 // not enforced so it should be big enough for any error
#ifdef DSL_SERVER_VERSION
#define NET_CLIENT_ADDR_BYTES 32
#define NET_CLIENT_KICK_BYTES 256
#else
#define NET_MAX_HOST_BYTES 128 // overkill for any domain name
#endif

// CLIENT MESSAGES (-> SERVER)
#define NET_MSG_LISTING 0 // dsl version, net sig, username
#define NET_MSG_CONNECT 1 // dsl version, net sig, username
#define NET_MSG_ALRIGHT 2 // nothing else encoded, got some file message
#define NET_MSG_IMREADY 3 // nothing else encoded, scripts started and ready to play
#define NET_MSG_SEVENTS 4 // null terminated string for event name, serialized lua data
#define NET_MSG_IMALIVE 5 // nothing else encoded
#define NET_MSG_SEVENTS2 6 // like NET_MSG_SEVENTS but the single argument *is* the serialized table

// SERVER MESSAGES (-> CLIENT)
#define NET_SVM_FUCK_YOU 0 // rest of message is a string for the kick reason
#define NET_SVM_WHATS_UP 1 // ask client what they want, nothing else encoded
#define NET_SVM_LIST_SERVER 2 // null terminated name, null terminated info, null terminated game mode, icon bytes, icon, players, max players
#define NET_SVM_START_FILE 3 // name of file
#define NET_SVM_APPEND_FILE 4 // some amount of file data to append to the file currently being built
#define NET_SVM_FINISH_FILE 5 // nothing else encoded, just tells client to close the file
#define NET_SVM_ADD_RESTART 6 // name of script collection to restart
#define NET_SVM_START_PLAYING 7 // all active collections as null terminated strings
#define NET_SVM_SCRIPT_EVENTS 8 // same encoding as NET_MSG_SEVENTS
#define NET_SVM_ARE_YOU_ALIVE 9 // nothing else encoded, sort of a "heartbeat" to avoid timeout
#define NET_SVM_SCRIPT_EVENTS2 10 // same encoding as NET_MSG_SEVENTS2

// SAFETY
#define NET_SAFE_EXTENSIONS "bin","ini","log","lua","lur","png","txt"

// MISCELLANEOUS
#define NET_TIMEOUT 8000
#define NET_HEARTBEAT 1000 // send a basic ping/pong message if nothing has been sent for this long while playing
#define NET_ARG_LIMIT 100 // maximum arguments for network script event
#define NET_SIGNATURE DSL_VERSION_NET
//#define NET_PRINT_DEBUG_MESSAGES
//#define NET_PRINT_DEBUG_MESSAGES_EX // also include the extra spammy debug messages

// TYPES
typedef struct network_state network_state;
typedef uint32_t net_msg_size;
#ifdef DSL_SERVER_VERSION
typedef struct network_player{
	int flags;
	SOCKET client;
	DWORD interacted;
	struct file_update *files; // NULL unless doing a file update
	char name[NET_MAX_USERNAME_BYTES];
	char address[NET_CLIENT_ADDR_BYTES];
	char shouldkick[NET_CLIENT_KICK_BYTES];
	char send[NET_MAX_MESSAGE_BUFFER];
	int sndi;
	char recv[NET_MAX_MESSAGE_BUFFER];
	int rcvi;
}network_player;
struct network_state{
	char name[NET_MAX_NAME_LENGTH+1];
	char info[NET_MAX_INFO_LENGTH+1];
	char icon[NET_MAX_ICON_BYTES]; // png to send to clients for listing
	size_t iconsize;
	char ip[NET_MAX_IP_LENGTH+1];
	char port[NET_MAX_PORT_LENGTH+1];
	char error[NET_MAX_ERROR_BUFFER]; // stores errors in deeper functions to be written later
	network_player *players; // allocated when config is read
	network_player *invalid_player; // for iteration
	unsigned max_players;
	SOCKET listen;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

// DEBUG
#ifdef NET_PRINT_DEBUG_MESSAGES
#define debugNetworkMessage printConsoleOutput
#else
#define debugNetworkMessage(...)
#endif
#ifdef NET_PRINT_DEBUG_MESSAGES_EX
#define debugNetworkMessageEx debugNetworkMessage
#else
#define debugNetworkMessageEx(...)
#endif

// COMMON
network_state *setupNetworking(struct dsl_state *dsl);
void cleanupNetworking(struct dsl_state *dsl,network_state *net);
void updateNetworking(struct dsl_state *dsl,network_state *net);
void updateNetworking2(struct dsl_state *dsl,network_state *net);

// SHARED
int checkNetworkExtension(const char *name);
int sendNetworkMessages(struct dsl_state *dsl,SOCKET s,char *identifier,char *buffer,int *index);
int recvNetworkMessages(struct dsl_state *dsl,SOCKET s,char *identifier,char *buffer,int *index,int (*process)(dsl_state*,void*,net_msg_size,char*),void *arg);

#ifdef DSL_SERVER_VERSION

// SERVER
void updateNetworkPlayerFiles(struct dsl_state *dsl);
network_player* getNetworkPlayerById(network_state *net,unsigned id); // only returns a connected player or NULL
int isNetworkPlayerConnected(network_player *player,int playing); // also optionally require them to be playing
void kickNetworkPlayer(lua_State *lua,dsl_state *dsl,network_player *player); // pops a string for the reason
void sendNetworkPlayerEvent(network_player *player,const char *name,const void *serialized,int bytes,int opt);
int areNetworkFilesTransfering(network_state *net); // if any file transfer is happening

#else

// CLIENT
int shouldNetworkDisableKeys(network_state *net);
int isNetworkPlayingOnServer(network_state *net);
int getNetworkSendBufferBytes(network_state *net); // -1 if not available
void updateNetworkPrompt(struct dsl_state *dsl,network_state *net); // draws update prompt
const char* requestNetworkListing(struct dsl_state *dsl,const char *ip,const char *port);
int promptNetworkServer(struct dsl_state *dsl,char *host,int printerror);
int disconnectFromPlaying(struct dsl_state *dsl);
void preparedNetworkScripts(struct dsl_state *dsl);
int sendNetworkServerEvent(network_state *net,const char *name,const void *serialized,int bytes,int opt);

#endif

#ifdef __cplusplus
}
#endif

#endif