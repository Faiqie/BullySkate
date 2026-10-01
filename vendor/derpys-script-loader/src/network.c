/* DERPY'S SCRIPT LOADER: NETWORKING
	
	shared networking utility
	
*/

#include <dsl/dsl.h>
#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

// POSIX PORT
#ifndef _WIN32
#define WSAGetLastError() errno
#endif

// DEBUG OPTIONS
//#define DEBUG_LIMIT_RECV 4

// FILE SAFETY
int checkNetworkExtension(const char *name){
	static const char *good[] = {NET_SAFE_EXTENSIONS};
	const char *ext;
	int count;
	
	ext = NULL;
	while(*name)
		if(*(name++) == '.')
			ext = name;
	if(!ext)
		return 1;
	count = sizeof(good) / sizeof(char*);
	while(count)
		if(!dslstrcmp(good[--count],ext))
			return 1;
	return 0;
}

// MESSAGE BUFFERS
static void removeBytes(char *buffer,int *index,int bytes){
	int count;
	int off;
	int i;
	
	count = *index;
	for(i = off = bytes;i < count;i++)
		buffer[i-off] = buffer[i];
	*index -= bytes;
}
int sendNetworkMessages(dsl_state *dsl,SOCKET s,char *identifier,char *buffer,int *index){
	int bytes;
	
	while(bytes = *index){
		bytes = send(s,buffer,bytes,0);
		if(!bytes) // shouldn't happen according to doc but we'll support it just in case
			break;
		if(bytes == SOCKET_ERROR){
			#ifdef _WIN32
			if((bytes = WSAGetLastError()) == WSAEWOULDBLOCK)
				break;
			if(bytes != WSAECONNRESET)
				printConsoleError(dsl->console,"{%s} failed to send data (wsa: %d)",identifier,bytes);
			#else
			if((bytes = WSAGetLastError()) == EAGAIN || bytes == EWOULDBLOCK)
				break;
			if(bytes != ECONNABORTED)
				printConsoleError(dsl->console,"{%s} failed to send data (reason: %s)",identifier,strerror(bytes));
			#endif
			return 1;
		}
		if(*index - bytes < 0){
			printConsoleError(dsl->console,"{%s} send buffer misbehaved",identifier);
			return 1;
		}
		removeBytes(buffer,index,bytes);
	}
	return 0; // zero = okay, non-zero = should disconnect
}
int recvNetworkMessages(dsl_state *dsl,SOCKET s,char *identifier,char *buffer,int *index,int (*process)(dsl_state*,void*,net_msg_size,char*),void *arg){
	net_msg_size size;
	int bytes;
	
	for(;;){
		#ifdef DEBUG_LIMIT_RECV
		bytes = NET_MAX_MESSAGE_BUFFER - *index;
		if(bytes > DEBUG_LIMIT_RECV)
			bytes = DEBUG_LIMIT_RECV;
		bytes = recv(s,buffer+*index,bytes,0);
		#else
		bytes = recv(s,buffer+*index,NET_MAX_MESSAGE_BUFFER-*index,0);
		#endif
		if(bytes == SOCKET_ERROR){
			#ifdef _WIN32
			if((bytes = WSAGetLastError()) == WSAEWOULDBLOCK)
				return 0; // zero = okay, non-zero = should disconnect
			if(bytes != WSAECONNRESET)
				printConsoleError(dsl->console,"{%s} failed to receive data (wsa: %d)",identifier,bytes);
			#else
			if((bytes = WSAGetLastError()) == EAGAIN || bytes == EWOULDBLOCK)
				return 0;
			if(bytes != ECONNABORTED)
				printConsoleError(dsl->console,"{%s} failed to receive data (reason: %s)",identifier,strerror(bytes));
			#endif
			return 1;
		}
		if(!bytes)
			return 1;
		*index += bytes;
		if(*index > NET_MAX_MESSAGE_BUFFER){
			printConsoleError(dsl->console,"{%s} receive buffer misbehaved",identifier);
			return 1;
		}
		while(*index >= sizeof(net_msg_size) && *index >= *(net_msg_size*)buffer){
			size = *(net_msg_size*)buffer;
			if(!size){
				printConsoleError(dsl->console,"{%s} received invalid event",identifier);
				return 1;
			}
			if((*process)(dsl,arg,size-sizeof(net_msg_size),buffer+sizeof(net_msg_size)))
				return 1;
			removeBytes(buffer,index,size);
		}
		if(*index >= NET_MAX_MESSAGE_BUFFER){
			printConsoleError(dsl->console,"{%s} receive buffer space was exhausted",identifier);
			return 1;
		}
	}
}