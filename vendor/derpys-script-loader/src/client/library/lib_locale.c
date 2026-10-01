// DERPY'S SCRIPT LOADER: LOCALE LIBRARY

#include <dsl/dsl.h>
#include <string.h>

#define CUSTOM_TEXT_OBJECT_NAME "DSL_TEXT"
#define PAIR_PREFIX_LENGTH 4 // idk why needed
#define TEXT_PREFIX_LENGTH 2 // -1

// TYPES
typedef struct text_pair{
	int hash;
	char *str; // starts with -1 -1
}text_pair;
typedef struct text_data{
	int abcd1234; // 0xABCD1234
	char unknown1[8];
	unsigned raw_bytes; // 129884
	unsigned pairs_bytes; // 28304
	int count; // 3538
	char unknown2[8];
	int something; // 17474
	char unknown3[4];
	text_pair *pairs;
	char *raw;
	int filehash;
	char unknown4[4];
}text_data;

// UTILITY
static int getMaxTextLength(text_data *data,text_pair *pair){
	int length;
	int reading;
	char *text;
	char *finish;
	
	length = 0;
	reading = 1;
	text = pair->str;
	finish = data->raw + data->raw_bytes;
	while(text + length < finish){
		if(reading)
			reading = text[length];
		else if(text[length])
			break; // hit the next string
		length++;
	}
	return length - TEXT_PREFIX_LENGTH - 1; // -1 for NUL
}
static text_pair *getTextPair(text_data *data,int hash){
	text_pair *pairs;
	int count;
	
	pairs = data->pairs;
	count = data->count;
	while(count)
		if(pairs[--count].hash == hash)
			return pairs + count;
	return NULL;
}
static text_data* getTextDataPtr(int hash){
	text_data **array;
	text_data *data;
	int i;
	
	array = (text_data**)0x20C2FA4;
	for(i = 0;i < 7;i++)
		if(array[i] && array[i]->filehash == hash)
			return array[i];
	while(i)
		if(!array[--i]){
			data = allocateGameMemory(sizeof(text_data));
			if(!data)
				return NULL;
			setupGameText(data,hash);
			data->raw = NULL;
			data->pairs = NULL;
			data->count = 0;
			data->raw_bytes = 0;
			data->pairs_bytes = 0;
			return array[i] = data;
		}
	return NULL;
}
static void removeTextDataPtr(text_data *data){
	text_data **array;
	int i;
	
	array = (text_data**)0x20C2FA4;
	for(i = 0;i < 7;i++)
		if(array[i] == data){
			array[i] = NULL;
			break;
		}
	freeGameMemory(data);
}
static int addTextDataString(text_data *data,int hash,unsigned length){
	unsigned pairs_bytes;
	unsigned raw_bytes;
	text_pair *pairs;
	char *raw;
	size_t offset;
	int count;
	int index;
	
	pairs_bytes = data->pairs_bytes + sizeof(text_pair);
	pairs = allocateGameMemory(pairs_bytes + PAIR_PREFIX_LENGTH);
	if(!pairs)
		return 0;
	raw_bytes = data->raw_bytes + TEXT_PREFIX_LENGTH + length + 1;
	while(raw_bytes % 4)
		raw_bytes++;
	raw = allocateGameMemory(raw_bytes);
	if(!raw){
		freeGameMemory(pairs);
		return 0;
	}
	if(data->pairs){
		memcpy(pairs,(char*)data->pairs-PAIR_PREFIX_LENGTH,data->pairs_bytes+PAIR_PREFIX_LENGTH);
		freeGameMemory((char*)data->pairs-PAIR_PREFIX_LENGTH);
	}else
		memset(pairs,0,PAIR_PREFIX_LENGTH);
	if(data->raw){
		memcpy(raw,data->raw,data->raw_bytes);
		freeGameMemory(data->raw);
		offset = raw - data->raw;
	}
	(char*)pairs += PAIR_PREFIX_LENGTH;
	count = data->count;
	for(index = 0;index < count;index++)
		(char*)pairs[index].str += offset;
	pairs[index].hash = hash;
	pairs[index].str = raw + data->raw_bytes;
	memset(raw+data->raw_bytes,-1,TEXT_PREFIX_LENGTH);
	memset(raw+data->raw_bytes+TEXT_PREFIX_LENGTH,0,length+1); // zero out the entire string because the length system depends on it
	data->pairs_bytes = pairs_bytes;
	data->raw_bytes = raw_bytes;
	data->count = count + 1;
	data->pairs = pairs;
	data->raw = raw;
	return 1;
}

// FUNCTIONS
static int GetLocalizedText(lua_State *lua){
	text_data **array;
	text_pair *pair;
	int hash;
	int i;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	array = (text_data**)0x20C2FA4;
	hash = getGameHash(lua_tostring(lua,1));
	pair = NULL;
	i = 7;
	while(i)
		if(array[--i] && (pair = getTextPair(array[i],hash)))
			break;
	if(!pair)
		return 0;
	lua_pushstring(lua,pair->str+TEXT_PREFIX_LENGTH);
	return 1;
}
static int RegisterLocalizedText(lua_State *lua){
	dsl_state *dsl;
	text_data *general;
	text_pair *exists;
	unsigned length;
	int hash;
	
	dsl = getDslState(lua,1);
	if(~dsl->flags & DSL_USED_TEXT_REGISTER){
		printConsoleWarning(dsl->console,"%sA script is using the custom text registration system, which is very unstable and experimental at this time.",getDslPrintPrefix(dsl,1));
		dsl->flags |= DSL_USED_TEXT_REGISTER;
	}
	luaL_checktype(lua,1,LUA_TSTRING);
	luaL_checktype(lua,2,LUA_TNUMBER);
	hash = getGameHash(lua_tostring(lua,1));
	length = lua_tonumber(lua,2);
	general = getTextDataPtr(getGameHash(CUSTOM_TEXT_OBJECT_NAME));
	if(!general)
		luaL_error(lua,"failed to setup custom text");
	if(exists = getTextPair(general,hash)){
		if(getMaxTextLength(general,exists) >= length)
			return 0; // we already have it, or a bigger one.
		if(!general->raw)
			removeTextDataPtr(general); // remove the empty text data object if needed
		luaL_error(lua,"text label \"%s\" was already registered with a smaller length",lua_tostring(lua,1));
	}
	if(!addTextDataString(general,hash,length)){
		if(!general->raw)
			removeTextDataPtr(general);
		luaL_error(lua,"failed to register text");
	}
	return 0;
}
static int ReplaceLocalizedText(lua_State *lua){
	text_data **array;
	text_pair *pair;
	size_t length;
	int hash;
	int i;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	array = (text_data**)0x20C2FA4;
	hash = getGameHash(lua_tostring(lua,1));
	pair = NULL;
	i = 7;
	while(i)
		if(array[--i] && (pair = getTextPair(array[i],hash)))
			break;
	if(!pair)
		luaL_error(lua,"no text label \"%s\" is currently loaded",lua_tostring(lua,1));
	length = getMaxTextLength(array[i],pair);
	memset(pair->str+TEXT_PREFIX_LENGTH,0,length+1);
	strncpy(pair->str+TEXT_PREFIX_LENGTH,luaL_checkstring(lua,2),length);
	return 0;
}

// OPEN
int dslopen_locale(lua_State *lua){
	lua_register(lua,"GetLocalizedText",&GetLocalizedText);
	lua_register(lua,"RegisterLocalizedText",&RegisterLocalizedText);
	lua_register(lua,"ReplaceLocalizedText",&ReplaceLocalizedText);
	return 0;
}