// DERPY'S SCRIPT SERVER: NETWORK LIBRARY

#include <dsl/dsl.h>
#include <string.h>

// UTILITY
static network_player* getPlayer(lua_State *lua,int stack){
	network_player *player;
	
	luaL_checktype(lua,stack,LUA_TNUMBER);
	player = getNetworkPlayerById(getDslState(lua,1)->network,lua_tonumber(lua,stack));
	if(!player)
		luaL_argerror(lua,stack,"invalid player");
	return player;
}

// FUNCTIONS
static int iterPlayers(lua_State *lua){
	network_player *player;
	network_state *net;
	unsigned max;
	unsigned id;
	int all;
	
	net = getDslState(lua,1)->network;
	max = net->max_players;
	all = lua_toboolean(lua,lua_upvalueindex(2));
	for(id = lua_tonumber(lua,lua_upvalueindex(1));id < max;id++)
		if((player = getNetworkPlayerById(net,id)) && (all || isNetworkPlayerConnected(player,1))){
			lua_pushnumber(lua,id+1);
			lua_replace(lua,lua_upvalueindex(1));
			lua_pushnumber(lua,id);
			return 1;
		}
	lua_pushnumber(lua,max);
	lua_replace(lua,lua_upvalueindex(1));
	return 0;
}
static int AllPlayers(lua_State *lua){
	lua_pushnumber(lua,0);
	if(lua_gettop(lua) > 1){
		luaL_checktype(lua,1,LUA_TBOOLEAN); // whether or not to allow players that are just connecting
		lua_settop(lua,2);
		lua_insert(lua,1); // put id first
	}else
		lua_pushboolean(lua,0);
	lua_pushcclosure(lua,&iterPlayers,2); // id, all
	return 1;
}
static int CheckPlayerArguments(lua_State *lua){
	network_player *player;
	int i,stack;
	
	// CheckPlayerArguments(player,value,type,value,type,...)
	stack = lua_gettop(lua);
	if(stack % 2)
		luaL_typerror(lua,stack+1,"expected type name");
	for(i = 1;i <= stack;i += 2)
		if(strcmp(lua_typename(lua,lua_type(lua,i)),lua_tostring(lua,i+1))){
			lua_pushstring(lua,"A script error occurred.");
			kickNetworkPlayer(lua,getDslState(lua,1),player);
			luaL_error(lua,"%s failed arg check #%d to `%s'",player->name);
		}
	return 0;
}
static int GetPlayerIp(lua_State *lua){
	lua_pushstring(lua,getPlayer(lua,1)->address);
	return 1;
}
static int GetPlayerName(lua_State *lua){
	lua_pushstring(lua,getPlayer(lua,1)->name);
	return 1;
}
static int GetServerHz(lua_State *lua){
	int hz;
	
	hz = getConfigInteger(getDslState(lua,1)->config,"server_hz");
	lua_pushnumber(lua,hz ? hz : NET_DEFAULT_HZ);
	return 1;
}
static int IsPlayerValid(lua_State *lua){
	network_player *player;
	
	if(lua_type(lua,1) == LUA_TNUMBER){
		if(lua_gettop(lua) >= 2)
			luaL_checktype(lua,2,LUA_TBOOLEAN);
		player = getNetworkPlayerById(getDslState(lua,1)->network,lua_tonumber(lua,1));
	}else
		player = NULL;
	lua_pushboolean(lua,player && (lua_toboolean(lua,2) || isNetworkPlayerConnected(player,1)));
	return 1;
}
static int KickPlayer(lua_State *lua){
	network_player *player;
	dsl_state *dsl;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	if(lua_gettop(lua) < 2)
		lua_pushstring(lua,"");
	luaL_checktype(lua,2,LUA_TSTRING);
	dsl = getDslState(lua,1);
	player = getNetworkPlayerById(dsl->network,lua_tonumber(lua,1));
	if(!player)
		luaL_argerror(lua,1,"invalid player");
	lua_settop(lua,2);
	kickNetworkPlayer(lua,dsl,player);
	return 0;
}
static int SendNetworkEvent(lua_State *lua){
	network_player *player;
	network_state *net;
	void *serialized;
	size_t bytes;
	unsigned id;
	int index;
	int stack;
	int opt;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	luaL_checktype(lua,2,LUA_TSTRING);
	if((stack = lua_gettop(lua)) == 3 && lua_istable(lua,3)){
		serialized = packLuaTableUserdata(lua,&bytes);
		if(!serialized)
			luaL_error(lua,lua_tostring(lua,-1));
		opt = 1;
	}else if(stack > 2){
		lua_newtable(lua);
		for(index = 3;index <= stack;index++){
			lua_pushvalue(lua,index);
			lua_rawseti(lua,-2,index-2);
		}
		stack -= 2; // so stack is now array count
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
	id = lua_tonumber(lua,1);
	if(id == -1){
		net = getDslState(lua,1)->network;
		for(id = 0;id < net->max_players;id++)
			if((player = getNetworkPlayerById(net,id)) && isNetworkPlayerConnected(player,1))
				sendNetworkPlayerEvent(player,lua_tostring(lua,2),serialized,bytes,opt);
	}else if((player = getNetworkPlayerById(getDslState(lua,1)->network,id)) && isNetworkPlayerConnected(player,1))
		sendNetworkPlayerEvent(player,lua_tostring(lua,2),serialized,bytes,opt);
	else
		luaL_error(lua,"invalid player");
	return 0;
}

// OPEN
int dslopen_server(lua_State *lua){
	lua_register(lua,"AllPlayers",&AllPlayers);
	//lua_register(lua,"CheckPlayerArguments",&CheckPlayerArguments); // unused because DSL shouldn't really add convenience functions that can easily be made in Lua
	lua_register(lua,"GetPlayerIp",&GetPlayerIp);
	lua_register(lua,"GetPlayerName",&GetPlayerName);
	lua_register(lua,"GetServerHz",&GetServerHz);
	lua_register(lua,"IsPlayerValid",&IsPlayerValid);
	lua_register(lua,"KickPlayer",&KickPlayer);
	lua_register(lua,"SendNetworkEvent",&SendNetworkEvent);
	return 0;
}