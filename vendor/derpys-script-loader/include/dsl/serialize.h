/* DERPY'S SCRIPT LOADER: LUA SERIALIZER
	
	two simple functions to pack and unpack basic lua data into binary
	tables cannot reference themselves (it will crash)
	
*/

#ifndef DSL_SERIALIZE_H
#define DSL_SERIALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

// PACK LUA TABLE
//  pops a table and tries encoding it for later unpacking.
//  packed data is returned and the size of it is put in bytes.
//  NULL is returned on failure and an error message is pushed on the stack.
//  if anything besides NULL is returned, it must be free'd when no longer needed.
#define packLuaTable(lua,bytes) packLuaTableAdvanced(lua,bytes,0)
void* packLuaTableAdvanced(lua_State *lua,size_t *bytes,int userdata);

// PACK LUA TABLE - USERDATA
//  works the same as packLuaTable but pushes userdata when successful.
//  userdata is used instead of normal memory functions so there's no need to free.
#define packLuaTableUserdata(lua,bytes) packLuaTableAdvanced(lua,bytes,1)

// UNPACK LUA TABLE
//  push a table onto the stack using previously packed data.
//  zero is returned on failure and an error message is pushed on the stack.
int unpackLuaTable(lua_State *lua,const void *data,size_t bytes);

#ifdef __cplusplus
}
#endif

#endif