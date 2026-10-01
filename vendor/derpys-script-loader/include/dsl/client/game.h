/* DERPY'S SCRIPT LOADER: GAME STATE FUNCTIONS AND MACROS
	
	this file has various macros to query game state
	it also has some game functions and structs
	
*/

#ifndef DSL_GAME_H
#define DSL_GAME_H

// General.
#define getGameTimer() (*(unsigned*)0xC1A9B4)
#define getGameWindow() (*(HWND*)0xBD77FC)
#define getGamePaused() (*(char*)0xC1A99A)
#define getGameMinimized() (*(char*)0xC1A998)
#define getGameDirect3D9() (*(IDirect3D9**)0x20E6D04)

// Render size.
#define getGameSize() ((int*)0xBCBC60)
#define getGameWidth() (*(int*)0xBCBC60)
#define getGameHeight() (*(int*)0xBCBC64)

// Script manager.
#define getGameScriptManager() ((void*)0xD02850)
#define getGameLuaState(state) (*(lua_State**)((char*)(state)+0x6B60))
#define getGameScriptPool(state) ((void**)((char*)(state)+0x6B64))
#define getGameScriptCount(state) (*(int*)((char*)(state)+0x6B84))
#define setGameScriptCount(state,count) (*(int*)((char*)(state)+0x6B84) = (count))
#define getGameScriptIndex(state) (*(int*)((char*)(state)+0x6B88))
#define setGameScriptIndex(state,index) (*(int*)((char*)(state)+0x6B88) = (index))

// Game controllers.
typedef struct game_joy{
	char zero1[0x40];
	float x;
	float y;
	float x2;
	float y2;
	char zero2[0x40];
	char sticks[0x10];
	int changes;
	short buttons; // bits
	short rawsticks[4];
}game_joy;
typedef struct game_input{
	game_joy joystick;
	DIMOUSESTATE mouse;
	char keyboard[0x100];
}game_input;
typedef struct game_controller{
	char is_joy;
	char unknown1[0x137];
	int id;
	int is_key;
	int timer;
	game_input input;
	char unknown2[0x1F4];
	game_input last_input;
	char unknown3[0x1F2];
	int released;
	int pressed;
	char unknown4[0xC];
	int sequence_buttons[15];
	int sequence_index;
	char unknown5[0x14];
}game_controller;
#define getGameControllers() ((game_controller*)0x20CECF0)
#define getGamePrimaryControllerIndex() (*(int*)0x20CECE8)
#define getGameSecondaryControllerIndex() (*(int*)0xBC7490)
#define getGameBindingsBasic() ((int*)0xA14810)
#define getGameBindingsAdvanced(arg) (*(int*(__fastcall*)(void*,void*,int))0x454CD0)((void*)0xA14810,NULL,(arg))
#define getGameStickInputMultiplier() (0.0078125**(float*)0xA1491C)

// Game pools.
#define GAME_POOL_INVALID 0x80
typedef struct game_pool{
	char *array; // not a string just a byte ptr
	unsigned char *flags;
	unsigned limit;
	unsigned size; // per element in array
	int unknown2;
	unsigned unknown3;
	int unknown4[2];
}game_pool;
#define getGamePoolObject(id,index) (*(void*(__cdecl*)(int,int))0x44A3E0)((id),(index))

// Pool specifics.
#define getGamePedPool() (*(game_pool**)0xC0F5F0)
#define getGameVehiclePool() (*(game_pool**)0xC0F5F4)

// Ped utility.
#define createGameScriptPed(lua,m,x,y,z,h) (*(int(__cdecl*)(lua_State*,int,float,float,float,float,int))0x5CF810)(lua,m,x,y,z,h,1)
#define getGamePedFromId(id,arg) (*(void*(__cdecl*)(int,int))0x5C7380)((id),(arg))
#define getGamePedId(ped) (*(int*)((char*)(ped)+0x1D2C))

// Script functions.
#define callGameLuaFunctionCreateThread(lua) (*(lua_CFunction)0x5BF450)(lua)
#define callGameLuaFunctionImportScript(lua) (*(lua_CFunction)0x5BE750)(lua)
#define callGameLuaFunctionPedSetPosXYZ(lua) (*(lua_CFunction)0x5C83B0)(lua)
#define callGameLuaFunctionTerminateCurrentScript(lua) (*(lua_CFunction)0x5BF4D0)(lua)
#define callGameLuaFunctionTerminateThread(lua) (*(lua_CFunction)0x5BF490)(lua)
#define callGameLuaFunctionWait(lua) (*(lua_CFunction)0x5BFBD0)(lua)

// Camera manager.
#define getGameCameraManager() ((void*)0xC3CC68)
#define getGameCameraObject(id) (*((void**)((char*)getGameCameraManager()+0x22C)+id))
#define setGameCameraObject(id) (*(void*(__fastcall*)(void*,void*,int))0x4F3590)(getGameCameraManager(),NULL,id)

// Memory functions.
#define allocateGameMemory(bytes) (*(void*(__cdecl*)(unsigned))0x5EEAA0)(bytes)
#define freeGameMemory(ptr) (*(void(__cdecl*)(void*))0x5EEAB0)(ptr)

// Miscellaneous functions.
#define cleanupGameScript(ptr) (*(void(__fastcall*)(void*))0x5DAAA0)(ptr)
#define isGameActionNodeValid(node) (*(int(__cdecl*)(const char*,void*))0x5F5B10)((node),NULL)
#define getGameHash(str) (*(int(__cdecl*)(const char*))0x576ED0)(str)
#define setupGameScript(ptr,name) (*(void*(__fastcall*)(void*,void*,const char*))0x5D8AB0)((ptr),NULL,(name))
#define setupGameText(ptr,hash) (*(void(__fastcall*)(void*,void*,int))0x6901E0)((ptr),NULL,(hash))

#endif