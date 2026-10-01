/* DERPY'S SCRIPT LOADER: LUA SERIALIZER
	
	two simple functions to pack and unpack basic lua data into binary
	tables cannot reference themselves (it will crash)
	
*/

#include <dsl/dsl.h>
#include <string.h>

// TODO: maybe add a simple counter to recursive table packing to detect tables that refer back to themselves?

// PACK
static int packTable(lua_State*,char**,size_t*);
static int packValue(lua_State *lua,char **data,size_t *bytes){
	size_t length;
	int type;
	
	type = lua_type(lua,-1);
	if(data)
		*(*data)++ = type;
	else
		(*bytes)++;
	if(type == LUA_TTABLE)
		return packTable(lua,data,bytes);
	if(type == LUA_TBOOLEAN){
		if(data)
			*(*data)++ = lua_toboolean(lua,-1);
		else
			(*bytes)++;
	}else if(type == LUA_TNUMBER){
		if(data){
			//*((lua_Number*)*data)++ = lua_tonumber(lua,-1);
			*(lua_Number*)*data = lua_tonumber(lua,-1);
			*data += sizeof(lua_Number);
		}else
			*bytes += sizeof(lua_Number);
	}else if(type == LUA_TSTRING){
		if(data){
			//*((size_t*)*data)++ = length = lua_strlen(lua,-1);
			*(size_t*)*data = length = lua_strlen(lua,-1);
			*data += sizeof(size_t);
			memcpy(*data,lua_tostring(lua,-1),length);
			*data += length;
		}else
			*bytes += sizeof(size_t) + lua_strlen(lua,-1);
	}else if(type == LUA_TLIGHTUSERDATA){
		if(data){
			//*((void**)*data)++ = lua_touserdata(lua,-1);
			*(void**)*data = lua_touserdata(lua,-1);
			*data += sizeof(void*);
		}else
			*bytes += sizeof(void*);
	}else
		return type;
	return 0;
}
static int packTable(lua_State *lua,char **data,size_t *bytes){
	unsigned array;
	unsigned pairs;
	unsigned index;
	float indexf;
	void *sizes;
	int type;
	
	if(data){
		sizes = *data;
		*data += sizeof(unsigned) * 2;
	}else
		*bytes += sizeof(unsigned) * 2;
	for(array = 1;lua_rawgeti(lua,-1,array),!lua_isnil(lua,-1);lua_pop(lua,1),array++)
		if(type = packValue(lua,data,bytes)){
			lua_pop(lua,1);
			return type;
		}
	array--;
	pairs = 0;
	while(lua_next(lua,-2)){ // nil was kept from the array part to start traversal
		if(lua_type(lua,-2) != LUA_TNUMBER || (index = indexf = lua_tonumber(lua,-2)) < 1 || index > array || index != indexf){
			lua_insert(lua,-2); // put key at top of stack
			if(type = packValue(lua,data,bytes)){
				lua_pop(lua,2);
				return type;
			}
			lua_insert(lua,-2); // put value at top of stack
			if(type = packValue(lua,data,bytes)){
				lua_pop(lua,1);
				return type;
			}
			pairs++;
		}
		lua_pop(lua,1); // pop value
	}
	if(data){
		*((unsigned*)sizes) = array;
		*((unsigned*)sizes+1) = pairs;
	}
	return 0;
}
void* packLuaTableAdvanced(lua_State *lua,size_t *result,int userdata){
	size_t bytes;
	char *data;
	char *arg;
	int type;
	
	bytes = 0;
	if(type = packTable(lua,NULL,&bytes)){ // stage 1: count required bytes
		lua_pop(lua,1);
		lua_pushstring(lua,"cannot pack unsupported type \"");
		lua_pushstring(lua,lua_typename(lua,type));
		lua_pushstring(lua,"\"");
		lua_concat(lua,3);
		return NULL;
	}
	if(userdata){
		data = lua_newuserdata(lua,bytes);
		lua_insert(lua,-2);
	}else{
		data = malloc(bytes);
		if(!data){
			lua_pop(lua,1);
			lua_pushstring(lua,"couldn't allocate memory for packing");
			return NULL;
		}
	}
	arg = data;
	if(packTable(lua,&arg,NULL)){ // stage 2: actually pack data
		if(userdata)
			lua_pop(lua,2);
		else{
			free(data);
			lua_pop(lua,1);
		}
		lua_pushstring(lua,"types changed between size calculation and packing");
		return NULL;
	}
	lua_pop(lua,1);
	*result = bytes;
	return data;
}

// UNPACK
static int unpackTable(lua_State*,char**,size_t*);
static int unpackBytes(size_t *bytes,size_t use){
	if(*bytes < use)
		return 1;
	*bytes -= use;
	return 0;
}
static int unpackValue(lua_State *lua,char **data,size_t *bytes){
	size_t length;
	int type;
	
	type = *(*data)++;
	if(type == LUA_TTABLE)
		return unpackTable(lua,data,bytes);
	if(type == LUA_TBOOLEAN){
		if(unpackBytes(bytes,1))
			return 1;
		lua_pushboolean(lua,*(*data)++);
	}else if(type == LUA_TNUMBER){
		if(unpackBytes(bytes,sizeof(lua_Number)))
			return 1;
		//lua_pushnumber(lua,*((lua_Number*)*data)++);
		lua_pushnumber(lua,*(lua_Number*)*data);
		*data += sizeof(lua_Number);
	}else if(type == LUA_TSTRING){
		if(unpackBytes(bytes,sizeof(size_t)))
			return 1;
		//length = *((size_t*)*data)++;
		length = *(size_t*)*data;
		*data += sizeof(size_t);
		if(unpackBytes(bytes,length))
			return 1;
		lua_pushlstring(lua,*data,length);
		*data += length;
	}else if(type == LUA_TLIGHTUSERDATA){
		if(unpackBytes(bytes,sizeof(void*)))
			return 1;
		//lua_pushlightuserdata(lua,*((void**)*data)++);
		lua_pushlightuserdata(lua,*(void**)*data);
		*data += sizeof(void*);
	}else
		return 1;
	return 0;
}
static int unpackTable(lua_State *lua,char **data,size_t *bytes){
	unsigned array;
	unsigned pairs;
	unsigned index;
	
	lua_newtable(lua);
	if(unpackBytes(bytes,sizeof(unsigned)*2)){ // require enough bytes for table counts
		lua_pop(lua,1);
		return 1;
	}
	//array = *((unsigned*)*data)++;
	array = *(unsigned*)*data;
	*data += sizeof(unsigned);
	//pairs = *((unsigned*)*data)++;
	pairs = *(unsigned*)*data;
	*data += sizeof(unsigned);
	if(unpackBytes(bytes,array+pairs*2)){ // require enough bytes for each type
		lua_pop(lua,1);
		return 1;
	}
	for(index = 1;index <= array;index++){
		if(unpackValue(lua,data,bytes)){
			lua_pop(lua,1);
			return 1;
		}
		lua_rawseti(lua,-2,index);
	}
	for(;pairs;pairs--){
		if(unpackValue(lua,data,bytes)){
			lua_pop(lua,1);
			return 1;
		}
		if(unpackValue(lua,data,bytes)){
			lua_pop(lua,2); // pop table + key
			return 1;
		}
		lua_rawset(lua,-3);
	}
	return 0;
}
int unpackLuaTable(lua_State *lua,const void *data,size_t bytes){
	if(!unpackTable(lua,(char**)&data,&bytes) && !bytes)
		return 1;
	lua_pushstring(lua,"cannot unpack corrupted data");
	return 0;
}