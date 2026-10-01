// DERPY'S SCRIPT LOADER: NETWORK LIBRARY

#include <dsl/dsl.h>

// UTILITY
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
static void assertNetwork(lua_State *lua,dsl_state *dsl){
	if(dsl->network)
		return;
	if(dsl->flags & DSL_SERVER_ACCESS)
		luaL_error(lua,"networking is not ready");
	luaL_error(lua,"networking is not allowed");
}

// FUNCTIONS
static int ConnectToServer(lua_State *lua){
	char ip[NET_MAX_HOST_BYTES];
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	assertNetwork(lua,dsl);
	luaL_checktype(lua,1,LUA_TSTRING);
	if(lua_strlen(lua,1) >= NET_MAX_HOST_BYTES)
		luaL_argerror(lua,1,"server address is too long");
	strcpy(ip,lua_tostring(lua,1));
	lua_pushboolean(lua,promptNetworkServer(dsl,ip,0));
	return 1;
}
static int DisconnectFromServer(lua_State *lua){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	assertNetwork(lua,dsl);
	lua_pushboolean(lua,!disconnectFromPlaying(dsl));
	return 1;
}
static int GetServerList(lua_State *lua){
	dsl_state *dsl;
	const char *sv;
	int array;
	
	dsl = getDslState(lua,1);
	assertNetwork(lua,dsl);
	lua_newtable(lua);
	array = 0;
	while(sv = getConfigStringArray(dsl->config,"list_server",array)){
		lua_pushstring(lua,sv);
		lua_rawseti(lua,-2,++array);
	}
	return 1;
}
static int IsNetworkingAllowed(lua_State *lua){
	lua_pushboolean(lua,getDslState(lua,1)->flags & DSL_SERVER_ACCESS);
	return 1;
}
static int RequestServerListing(lua_State *lua){
	char ip[NET_MAX_HOST_BYTES];
	const char *address;
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	assertNetwork(lua,dsl);
	luaL_checktype(lua,1,LUA_TSTRING);
	if(lua_strlen(lua,1) >= NET_MAX_HOST_BYTES)
		luaL_argerror(lua,1,"server address is too long");
	strcpy(ip,lua_tostring(lua,1));
	address = requestNetworkListing(dsl,ip,seperatePort(ip));
	if(!address)
		return 0;
	lua_pushstring(lua,address);
	return 1;
}
static int SendNetworkEvent(lua_State *lua){
	dsl_state *dsl;
	void *serialized;
	size_t bytes;
	int index;
	int stack;
	int opt;
	
	dsl = getDslState(lua,1);
	assertNetwork(lua,dsl);
	luaL_checktype(lua,1,LUA_TSTRING);
	if((stack = lua_gettop(lua)) == 2 && lua_istable(lua,2)){
		serialized = packLuaTableUserdata(lua,&bytes);
		if(!serialized)
			luaL_error(lua,lua_tostring(lua,-1));
		opt = 1;
	}else if(stack > 1){
		lua_newtable(lua);
		for(index = 2;index <= stack;index++){
			lua_pushvalue(lua,index);
			lua_rawseti(lua,-2,index-1);
		}
		stack--; // so stack is now array count
		if(luaL_getn(lua,-1) != stack){
			lua_pushstring(lua,"n");
			lua_pushnumber(lua,stack);
			lua_rawset(lua,-3);
		}
		serialized = packLuaTableUserdata(lua,&bytes);
		if(!serialized)
			luaL_error(lua,lua_tostring(lua,-1));
		opt = 0;
	}else
		bytes = 0;
	if(sendNetworkServerEvent(dsl->network,lua_tostring(lua,1),serialized,bytes,opt))
		luaL_error(lua,"not ready for network events");
	return 0;
}
static int GetNetworkSendBuffer(lua_State *lua){
	dsl_state *dsl;
	int bytes;
	
	dsl = getDslState(lua,1);
	assertNetwork(lua,dsl);
	bytes = getNetworkSendBufferBytes(dsl->network);
	if(bytes == -1)
		luaL_error(lua,"not connected to server");
	lua_pushnumber(lua,bytes);
	return 1;
}

// OPEN
int dslopen_client(lua_State *lua){
	lua_register(lua,"ConnectToServer",&ConnectToServer);
	lua_register(lua,"DisconnectFromServer",&DisconnectFromServer);
	lua_register(lua,"GetServerList",&GetServerList);
	lua_register(lua,"IsNetworkingAllowed",&IsNetworkingAllowed);
	lua_register(lua,"RequestServerListing",&RequestServerListing);
	lua_register(lua,"SendNetworkEvent",&SendNetworkEvent);
	if(getDslState(lua,1)->flags & DSL_ADD_DEBUG_FUNCS)
		lua_register(lua,"GetNetworkSendBuffer",&GetNetworkSendBuffer);
	return 0;
}