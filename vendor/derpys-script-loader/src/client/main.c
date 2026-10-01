/* DERPY'S SCRIPT LOADER: MAIN
	
	this is the entry point of the mod (DllMain)
	it's responsible for patching the game and controlling dsl.c
	
*/

#include <dsl/dsl.h>
#include <dsl/client/patch.h>
#include <wchar.h>

//#define START_DELAY 1 // initialize DSL after some amount of threads are created instead of immediately
//#define DEBUG_THREADS

// LUA FUNCTIONS
void luaC_collectgarbage();
void luaM_realloc();
void luaX_init();
void luaY_parser();

// LUA FUNCTIONS (unneeded)
void luaD_pcall();
void luaD_protectedparser();
void luaE_newthread();
void luaH_new();
void luaH_set();
void luaS_newlstr();
void luaV_execute();

// DSL CONTROLLER
void* openDsl(void *game,IDirect3DDevice9 *device,void *data);
void* saveDslData(void*); // called before closing DSL to get data to send to the next DSL
void closeDsl(void*);
void initDslContent(void*); // a good point to do content replacements
void updateDslKeyboard(void*,char*);
void updateDslMouse(void*,DIMOUSESTATE*);
void updateDslController(void*,void*);
void updateDslControllers(void*);
void updateDslBeforeSystem(void*);
void updateDslAfterSkipped(void*);
void updateDslAfterScripts(void*);
void updateDslAfterWorld(void*);
void updateDslBeforeFade(void*);
void updateDslBeforeDraw(void*);
void readDslSaveData(void*,void*,size_t); // sometimes late
int writeDslSaveData(void*,HANDLE);
void creatingDslPed(void*); // immediately before ped creation
int canDslBuildPlayer(void*,void*,void*);
int canDslSetPedStat(void*,void*,int,int);
void setDslCameraFade(void*,int,float);
int canDslLoadSaveGame(void*);
void loadedDslGameScript(void*,void*,lua_State*);
int canDslPauseGame(void*);
int canDslForceSleep(void*);
void setDslPedThrottle(void*,void*);
void updateDslWindow(void*,HWND,int);
int canDslBeMinimized(void*);
int canDslGoFullscreen(void*);

// GLOBAL VARIABLES
static int g_threadcount; // mod initializes on creation of a certain thread if START_DELAY is set
static int g_waitforsave; // loaded save data through menu and must hold it until lua close
static size_t *g_savedata; // extra save data at the end of save file to be given to dsl
static void *g_data; // persistent data that needs to be kept between DSL states
static void *g_dsl; // pointer returned by openDsl and passed to dsl functions
static void *g_controller; // pointer for controllers currently being updated
static IDirect3DDevice9 *g_d3d9device; // saved when d3d9 is setup
static unsigned char (__cdecl *g_originalbegin)(void*,int);
static void motionDiagnostic(const char *message,const void *address){
	FILE *file=fopen("_derpy_script_loader/logs/skate-window.log","a");
	if(file){fprintf(file,"%s %p\n",message,address);fclose(file);}
}
static void *motionCallTarget(void *address){
	unsigned char *call=(unsigned char*)address;
	return call[0]==0xE8 ? call+5+*(int32_t*)(call+1) : NULL;
}
static unsigned char __cdecl beginSkateScene(void *camera,int flags){
	unsigned char result=g_originalbegin(camera,flags);
	if(result){ extern void fakieBeforeWorld(void*); fakieBeforeWorld(g_dsl); }
	return result;
}

/* Bully consumes WM_QUIT in gameplay. Closing must exit on the window/game
   thread instead of waiting for its unused application message pump. */
static int g_close_requested;
static WNDPROC g_windowproc;
static HWND g_motionwindow;
static void motionExitGame(void){
	/* Windows closes the worker's kill-on-close job with the parent process. */
	ExitProcess(0);
}
static LRESULT CALLBACK motionWindowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
	if(message==WM_CLOSE || (message==WM_SYSCOMMAND && (wparam&0xFFF0)==SC_CLOSE) ||
	   (message==WM_SYSKEYDOWN && wparam==VK_F4 && (lparam&(1L<<29)))){
		g_close_requested=1;motionDiagnostic("Window close requested",window);motionExitGame();return 0;
	}
	return CallWindowProc(g_windowproc,window,message,wparam,lparam);
}
static void motionWindowHooks(HWND window){
	if(!window || g_motionwindow==window)return;
	g_windowproc=(WNDPROC)SetWindowLongPtr(window,GWLP_WNDPROC,(LONG_PTR)&motionWindowProc);
	g_motionwindow=window;
	motionDiagnostic("Original window procedure",g_windowproc);
}
static void motionCloseShortcut(void){
	HWND window=getGameWindow();
	motionWindowHooks(window);
	if(window && GetForegroundWindow()==window && (GetAsyncKeyState(VK_MENU)&0x8000) &&
	   (GetAsyncKeyState(VK_F4)&0x8000)){
		if(!g_close_requested)motionDiagnostic("Alt+F4 close requested",window);
		g_close_requested=1;
		motionExitGame();
	}
}
#ifdef DEBUG_THREADS
static DWORD g_threadid;
#endif

// THREAD TESTING
#ifdef DEBUG_THREADS
#define assertSameThread() assertSameThreadFunc(__FUNCTION__)
static void assertSameThreadFunc(const char *name){
	if(!g_threadid)
		g_threadid = GetCurrentThreadId();
	else if(g_threadid != GetCurrentThreadId())
		MessageBox(NULL,name,"different thread",MB_OK);
}
#else
#define assertSameThread()
#endif

// D3D INIT
static int motionWindowed(){
	char option[8];
	return GetEnvironmentVariableA("BULLY_SKATE_WINDOWED",option,sizeof(option)) && option[0]=='1';
}
static void motionPresentation(D3DPRESENT_PARAMETERS *parameters){
	if(!motionWindowed())return;
	parameters->Windowed=TRUE;
	parameters->FullScreen_RefreshRateInHz=0;
	parameters->BackBufferFormat=D3DFMT_UNKNOWN;
	parameters->SwapEffect=D3DSWAPEFFECT_DISCARD;
}
static void motionDisplayLog(const char *operation,HRESULT result,D3DPRESENT_PARAMETERS *p){
	FILE *file=fopen("_derpy_script_loader/logs/skate-display.log","a");
	if(file){fprintf(file,"%s hr=0x%08lX windowed=%d size=%ux%u\n",operation,(unsigned long)result,p->Windowed,p->BackBufferWidth,p->BackBufferHeight);fclose(file);}
}
void fakieBeforeDeviceReset(IDirect3DDevice9 *device);
static HRESULT (__stdcall *g_originalreset)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
static HRESULT __stdcall resetMotionDevice(IDirect3DDevice9 *device,D3DPRESENT_PARAMETERS *parameters){
	HRESULT result;
	fakieBeforeDeviceReset(device);
	motionPresentation(parameters);
	result=g_originalreset(device,parameters);
	motionDisplayLog("Reset",result,parameters);
	return result;
}
static HRESULT __stdcall setupDevice(void *arg,UINT adapter,D3DDEVTYPE type,HWND focus,DWORD behavior,D3DPRESENT_PARAMETERS *presentation,IDirect3DDevice9 **device){
	HRESULT result;
	
	assertSameThread();
	motionPresentation(presentation);
	result = IDirect3D9_CreateDevice(getGameDirect3D9(),adapter,type,focus,behavior,presentation,device);
	motionDisplayLog("CreateDevice",result,presentation);
	if(result == D3D_OK){
		if(motionWindowed()){
			RECT rect={0,0,(LONG)presentation->BackBufferWidth,(LONG)presentation->BackBufferHeight};
			DWORD style=WS_POPUP|WS_VISIBLE|WS_SYSMENU;
			AdjustWindowRect(&rect,style,FALSE);
			SetWindowLong(focus,GWL_STYLE,style);
			SetWindowPos(focus,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOZORDER|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
			ShowWindow(focus,SW_SHOWNORMAL);
			SetForegroundWindow(focus);
		}
		g_d3d9device = *device;
		if(!g_originalreset){
			void **table=*(void***)*device;
			g_originalreset=(void*)table[16];
			replaceImmediateValue(table+16,&resetMotionDevice);
		}
	}
	return result;
}

// GAME MAIN
static void __cdecl startGame(void *arg){
	WCHAR executable[MAX_PATH],*separator;DWORD length;
	assertSameThread();
	/* Direct launches and third-party launchers can inherit another directory.
	   Anchor DSL's relative config/assets to the actual Bully executable. */
	length=GetModuleFileNameW(NULL,executable,MAX_PATH);
	if(length>0&&length<MAX_PATH&&(separator=wcsrchr(executable,L'\\'))){
		*separator=0;SetCurrentDirectoryW(executable);
	}
	if(g_dsl = openDsl(NULL,NULL,NULL)){
		initDslContent(g_dsl);
		g_waitforsave = 1;
	}
	/* Chain the installed BeginScene target so other graphics fixes keep their
	   render pass. Do not replace the individual entity-render call sites. */
	g_originalbegin=(void*)motionCallTarget((void*)0x43CB16);
	motionDiagnostic("BeginScene target",g_originalbegin);
	motionDiagnostic("Entity render target",motionCallTarget((void*)0x43CB3A));
	motionDiagnostic("End entity render target",motionCallTarget((void*)0x43CCA1));
	if(g_originalbegin)replaceFunctionCall((void*)0x43CB16,5,&beginSkateScene);
	(*(void(__cdecl*)(void*))0x5EF0A0)(arg);
}

// GAME LOAD
static void __cdecl loadGame(char *world){
	assertSameThread();
	(*(void(__cdecl*)(char*))0x42EFE0)(world);
	if(g_dsl){
		g_data = saveDslData(g_dsl);
		closeDsl(g_dsl);
		g_dsl = NULL;
	}
	if(g_d3d9device && (g_dsl = openDsl(NULL,g_d3d9device,g_data)))
		g_waitforsave = 1;
}

// GAME INIT
static void __fastcall initLuaState(void *game){
	assertSameThread();
	if(g_dsl){
		g_data = saveDslData(g_dsl);
		closeDsl(g_dsl);
		g_dsl = NULL;
	}
	(*(void(__fastcall*)(void*))0x5DB3C0)(game);
	if(g_d3d9device){
		g_dsl = openDsl(game,g_d3d9device,g_data);
		if(g_dsl && g_savedata){
			readDslSaveData(g_dsl,g_savedata+1,*g_savedata);
			free(g_savedata);
			g_savedata = NULL;
		}
	}
}

// GAME LOOP
static void __cdecl updateSystem(){
	assertSameThread();
	motionCloseShortcut();
	if(g_dsl)
		updateDslBeforeSystem(g_dsl);
	(*(void(__cdecl*)())0x405680)();
}
static void __cdecl skipGameUpdate(void *arg){
	assertSameThread();
	if(g_dsl)
		updateDslAfterSkipped(g_dsl);
	(*(void(__cdecl*)(void*))0x45B960)(arg);
}
static void __cdecl updateController(void *controller){
	assertSameThread();
	(*(void(__cdecl*)(void*))0x738570)(controller);
	if(g_dsl && controller == (char*)0x20CECF0 + 0x914 * 3)
		updateDslControllers(g_dsl);
}
static int __fastcall updateInputs(void *controller){
	assertSameThread();
	g_controller = controller;
	return (*(int(__fastcall*)(void*))0x50CD30)(controller);;
}
static int __cdecl updateInputs2a(){
	assertSameThread();
	if(g_controller){
		if(g_dsl)
			updateDslController(g_dsl,g_controller);
		g_controller = NULL;
	}
	return (*(int(__cdecl*)())0x45BCA0)();
}
static DWORD __stdcall updateInputs2b(DWORD index,void *state){
	DWORD result;
	
	assertSameThread();
	result = ((DWORD(__stdcall*)(DWORD,void*))0x85AB96)(index,state);
	if(g_controller){
		if(g_dsl)
			updateDslController(g_dsl,g_controller);
		g_controller = NULL;
	}
	return result;
}
static void __fastcall updateScripts(void *game,void *edx,void *arg){
	assertSameThread();
	(*(void(__fastcall*)(void*,void*,void*))0x5DBB40)(game,edx,arg);
	if(g_dsl)
		updateDslAfterScripts(g_dsl);
}
static void __cdecl drawGameWorld(){
	assertSameThread();
	{ extern void fakieAfterWorld(void); fakieAfterWorld(); }
	(*(void(__cdecl*)())0x55E2C0)();
	{ extern void fakieDrawWorld(void*); fakieDrawWorld(g_dsl); }
	if(g_dsl)
		updateDslAfterWorld(g_dsl);
}
static void __cdecl fadeGameScreen(void *a,void *b){
	assertSameThread();
	if(g_dsl)
		updateDslBeforeFade(g_dsl);
	(*(void(__cdecl*)(void*,void*))0x43C060)(a,b);
}
static void __cdecl updateDrawing(void *arg){
	assertSameThread();
	if(g_dsl)
		updateDslBeforeDraw(g_dsl);
	(*(void(__cdecl*)(void*))0x43ACE0)(arg); // also calls presentGame
}
/*
static HRESULT __stdcall presentGame(IDirect3DDevice9 *device){
	assertSameThread();
	if(!g_d3d9device)
		g_d3d9device = device;
	return IDirect3DDevice9_Present(device,NULL,NULL,NULL,NULL);
}
*/

// GAME CLEANUP
static void __cdecl destroyLuaState(lua_State *lua){
	assertSameThread();
	if(g_dsl)
		g_data = saveDslData(g_dsl);
	(*(void(__cdecl*)(lua_State*))0x7420B0)(lua); // lua_close
	if(g_dsl){
		closeDsl(g_dsl);
		g_dsl = NULL;
	}
	g_waitforsave = 0;
}

// GAME SAVING
static DWORD __cdecl readSaveFile(HANDLE file,LPVOID buffer,DWORD bytes){
	DWORD extra,read;
	
	assertSameThread();
	if(g_savedata){
		free(g_savedata);
		g_savedata = NULL;
	}
	if(!ReadFile(file,buffer,bytes,&bytes,NULL))
		return 0;
	extra = GetFileSize(file,NULL);
	if(extra == INVALID_FILE_SIZE)
		return 0;
	if(extra < bytes)
		return bytes;
	extra -= bytes;
	g_savedata = malloc(sizeof(size_t) + extra);
	if(!g_savedata)
		return 0;
	*g_savedata = extra;
	if(!ReadFile(file,g_savedata+1,extra,&read,NULL) || read != extra){
		free(g_savedata);
		g_savedata = NULL;
		return 0;
	}
	if(g_dsl && !g_waitforsave){
		readDslSaveData(g_dsl,g_savedata+1,*g_savedata);
		free(g_savedata);
		g_savedata = NULL;
	}
	return bytes;
}
static DWORD __cdecl readSaveFile2(HANDLE file,LPVOID buffer,DWORD bytes){
	assertSameThread();
	g_waitforsave = 1;
	return readSaveFile(file,buffer,bytes);
}
static BOOL __cdecl writeSaveFile(HANDLE file,LPCVOID buffer,DWORD bytes){
	DWORD write;
	
	assertSameThread();
	if(!WriteFile(file,buffer,bytes,&bytes,NULL))
		return 0;
	if(g_dsl){
		if(g_savedata){
			free(g_savedata);
			g_savedata = NULL;
		}
		if(!writeDslSaveData(g_dsl,file))
			return 0;
	}else if(g_savedata && (!WriteFile(file,g_savedata+1,*g_savedata,&write,NULL) || write != bytes))
		return 0;
	return bytes;
}

// WINDOW STUFF
static int __cdecl updateWindow(){
	int result;
	
	assertSameThread();
	result = (*(int(__cdecl*)())0x405F60)();
	if(g_dsl)
		updateDslWindow(g_dsl,getGameWindow(),1);
	return result;
}
static void __cdecl unfocusWindow(int focus){
	assertSameThread();
	if(!g_dsl || canDslBeMinimized(g_dsl))
		(*(void(__cdecl*)(int))0x405DB0)(focus);
}
static BOOL __stdcall showAfterWindowCreated(HWND window,int show){
	assertSameThread();
	motionWindowHooks(window);
	if(g_dsl)
		updateDslWindow(g_dsl,window,0);
	return ShowWindow(window,show);
}
static LONG __stdcall changeDisplaySettings(DEVMODEA *mode,DWORD flags){
	assertSameThread();
	if(motionWindowed())return DISP_CHANGE_SUCCESSFUL;
	if(g_dsl && !canDslGoFullscreen(g_dsl))
		return DISP_CHANGE_SUCCESSFUL;
	return ChangeDisplaySettingsA(mode,flags);
}

// MISCELLANEOUS
static void* __cdecl creatingPed(int arg){
	assertSameThread();
	if(g_dsl)
		creatingDslPed(g_dsl);
	return (*(void*(__cdecl*)(int))0x4789E0)(arg);
}
static void __fastcall buildPlayer(void *player,void *edx,void *arg){
	assertSameThread();
	if(!g_dsl || canDslBuildPlayer(g_dsl,player,arg))
		(*(void*(__fastcall*)(void*,void*,void*))0x6CC7C0)(player,edx,arg);
}
static void __fastcall setPedStat(void *ped,void *edx,int stat,int value){
	assertSameThread();
	if(!g_dsl || canDslSetPedStat(g_dsl,ped,stat,value))
		(*(void(__fastcall*)(void*,void*,int,int))0x4770F0)(ped,edx,stat,value);
}
static void __fastcall fadeCamera(void *manager,void *edx,float timer,int black,int arg){
	assertSameThread();
	(*(void(__fastcall*)(void*,void*,float,int,int))0x4F1AD0)(manager,edx,timer,black,arg);
	if(g_dsl)
		setDslCameraFade(g_dsl,black,timer);
}
static void* __cdecl loadSaveGame(char *name){
	assertSameThread();
	if(g_dsl && !canDslLoadSaveGame(g_dsl))
		return NULL;
	return (*(void*(__cdecl*)(void*))0x7357A0)(name);
}
static void __fastcall loadLuaScript(void *script,void *edx,lua_State *lua){
	assertSameThread();
	(*(void(__fastcall*)(void*,void*,lua_State*))0x5D9DB0)(script,edx,lua);
	if(g_dsl)
		loadedDslGameScript(g_dsl,script,lua);
}
static void __cdecl setGamePaused(){
	assertSameThread();
	if(!g_dsl || canDslPauseGame(g_dsl))
		(*(void(__cdecl*)())0x45B810)();
}
static int __fastcall shouldAllowSleep(void *arg){
	assertSameThread();
	if(g_dsl && !canDslForceSleep(g_dsl))
		return 1;
	return (*(int(__fastcall*)(void*))0x4F12F0)(arg);
}
static int __fastcall setPedThrottle(void *ecx,void *edx,float arg){
	char *something;
	void **func;
	int result;
	
	assertSameThread();
	something = *(char**)ecx;
	func = *(void**)(something+0xC);
	result = (*(int(__fastcall*)(void*,void*,float))func)(ecx,edx,arg);
	if(g_dsl && result && (func == (void*)0x5F7ED0 || func == (void*)0x60AF60) && (something = *(char**)((char*)ecx+0x40)) && (something = *(void**)(something+8)))
		setDslPedThrottle(g_dsl,something);
	return result;
}
static void __fastcall beforeControllerProcessing(char *controller,void *edx,void *arg,void *input){
	assertSameThread();
	if(g_dsl && *(int*)(controller+0x138) == 0){
		updateDslKeyboard(g_dsl,(char*)controller+0x204);
		updateDslMouse(g_dsl,(DIMOUSESTATE*)(controller+0x1F4));
	}
	(*(void(__fastcall*)(void*,void*,void*,void*))0x50C030)(controller,edx,arg,input);
}
static HRESULT __stdcall setKeyboardCooperativeLevel(IDirectInputDevice8 *device,HWND window,DWORD flags){
	assertSameThread();
	return IDirectInputDevice8_SetCooperativeLevel(device,window,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND);
}

// MAIN
static BOOL DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){
	#ifdef START_DELAY
	if(reason == DLL_THREAD_ATTACH && g_threadcount != START_DELAY && (++g_threadcount) == START_DELAY){
	#else
	if(reason == DLL_PROCESS_ATTACH){
	#endif
		// patch lua (gc, realloc, and state functions):
		replaceCodeWithJump(&lua_close,(void*)0x7420B0);
		replaceCodeWithJump(&lua_newthread,(void*)0x73AE60);
		replaceCodeWithJump(&lua_open,(void*)0x742010);
		replaceCodeWithJump(&luaC_collectgarbage,(void*)0x740F20);
		//replaceCodeWithJump(&luaM_realloc,(void*)0x7416F0);
		replaceCodeWithJump((void*)0x7416F0,&luaM_realloc);
		// lua parser:
		replaceFunctionCall((void*)0x741E4A,5,&luaX_init);
		replaceFunctionCall((void*)0x73FFEB,5,&luaY_parser);
		// d3d init:
		replaceFunctionCall((void*)0x880117,5,&setupDevice);
		// game start:
		replaceFunctionCall((void*)0x43E8BE,5,&startGame);
		// game load:
		replaceFunctionCall((void*)0x43CF54,5,&loadGame);
		// game init:
		replaceFunctionCall((void*)0x5DC374,5,&initLuaState);
		// game loop:
		replaceFunctionCall((void*)0x43D650,5,&updateSystem);
		replaceFunctionCall((void*)0x43D660,5,&skipGameUpdate);
		replaceFunctionCall((void*)0x738617,5,&updateController);
		replaceFunctionCall((void*)0x73857A,5,&updateInputs); // inside updateController, and updateInputs2* are inside this
		replaceFunctionCall((void*)0x50D5C2,5,&updateInputs2a); // copy non-controller input
		replaceFunctionCall((void*)0x50CD6D,5,&updateInputs2b); // copy controller input
		replaceFunctionCall((void*)0x43007F,5,&updateScripts);
		replaceFunctionCall((void*)0x43CCA6,5,&drawGameWorld);
		replaceFunctionCall((void*)0x43CED2,5,&fadeGameScreen);
		replaceFunctionCall((void*)0x43CEE6,5,&updateDrawing);
		//replaceFunctionCallWithPrefix((void*)0x894348,11,0x50,&presentGame);
		// game cleanup:
		replaceFunctionCall((void*)0x5DBA4A,5,&destroyLuaState);
		// game saving:
		replaceFunctionCall((void*)0x735975,5,&readSaveFile); // read save when "story mode" is hit
		replaceFunctionCall((void*)0x7372B9,5,&readSaveFile2); // read save from the pause menu
		replaceFunctionCall((void*)0x7367EF,5,&writeSaveFile); // write save file data
		// window stuff:
		replaceFunctionCall((void*)0x4063BA,5,&updateWindow);
		replaceFunctionCall((void*)0x401106,5,&unfocusWindow);
		replaceFunctionCall((void*)0x82BB70,6,&showAfterWindowCreated);
		replaceFunctionCall((void*)0x4055B8,6,&changeDisplaySettings);
		// pool creation:
		replaceFunctionCall((void*)0x456F4A,5,&creatingPed);
		replaceFunctionCall((void*)0x49B576,5,&creatingPed);
		replaceFunctionCall((void*)0x49F5C8,5,&creatingPed);
		replaceFunctionCall((void*)0x49F7CC,5,&creatingPed);
		replaceFunctionCall((void*)0x49FDB6,5,&creatingPed);
		replaceFunctionCall((void*)0x4A2700,5,&creatingPed);
		replaceFunctionCall((void*)0x5CF8B9,5,&creatingPed);
		replaceFunctionCall((void*)0x5D0552,5,&creatingPed);
		// build player:
		replaceFunctionCall((void*)0x4062C6,5,&buildPlayer);
		replaceFunctionCall((void*)0x4062E6,5,&buildPlayer);
		replaceFunctionCall((void*)0x5BB0B1,5,&buildPlayer);
		replaceFunctionCall((void*)0x6C5541,5,&buildPlayer);
		replaceFunctionCall((void*)0x6C6108,5,&buildPlayer);
		replaceFunctionCall((void*)0x6CCDA3,5,&buildPlayer);
		replaceFunctionCall((void*)0x73503E,5,&buildPlayer);
		// stat override:
		replaceFunctionCall((void*)0x487A23,5,&setPedStat); // set stat 8
		replaceFunctionCall((void*)0x5C9212,5,&setPedStat);
		replaceFunctionCall((void*)0x5D4EB8,5,&setPedStat);
		replaceFunctionCall((void*)0x67C4F9,5,&setPedStat);
		replaceFunctionCall((void*)0x67C526,5,&setPedStat);
		replaceFunctionCall((void*)0x67C538,5,&setPedStat);
		replaceFunctionCall((void*)0x67C8B7,5,&setPedStat);
		// fade camera:
		replaceFunctionCall((void*)0x41711A,5,&fadeCamera);
		replaceFunctionCall((void*)0x417509,5,&fadeCamera);
		replaceFunctionCall((void*)0x4314DA,5,&fadeCamera);
		replaceFunctionCall((void*)0x43ACBA,5,&fadeCamera);
		replaceFunctionCall((void*)0x43BE79,5,&fadeCamera);
		replaceFunctionCall((void*)0x43D632,5,&fadeCamera);
		replaceFunctionCall((void*)0x43D6C3,5,&fadeCamera);
		replaceFunctionCall((void*)0x43E7B8,5,&fadeCamera);
		replaceFunctionCall((void*)0x4F82C4,5,&fadeCamera);
		replaceFunctionCall((void*)0x5B9077,5,&fadeCamera);
		replaceFunctionCall((void*)0x65D26D,5,&fadeCamera);
		replaceFunctionCall((void*)0x65D282,5,&fadeCamera);
		replaceFunctionCall((void*)0x69E580,5,&fadeCamera);
		replaceFunctionCall((void*)0x6AA0DB,5,&fadeCamera);
		replaceFunctionCall((void*)0x6AA12A,5,&fadeCamera);
		replaceFunctionCall((void*)0x7052A3,5,&fadeCamera);
		replaceFunctionCall((void*)0x70770D,5,&fadeCamera);
		// miscellaneous:
		replaceFunctionCall((void*)0x7375C9,5,&loadSaveGame);
		replaceFunctionCall((void*)0x5DC154,5,&loadLuaScript);
		replaceFunctionCall((void*)0x6A3D22,5,&setGamePaused);
		replaceFunctionCall((void*)0x445F75,5,&shouldAllowSleep);
		replaceImmediateByte((void*)0x5F43EC,0xBA); // mov edx, &setPedThrottle
		replaceImmediateValue((void*)0x5F43ED,&setPedThrottle);
		replaceFunctionCall((void*)0x50D29C,5,&beforeControllerProcessing);
		replaceFunctionCall((void*)0x50BC66,5,&setKeyboardCooperativeLevel);
		// unneeded:
		/*
		replaceCodeWithJump(&luaD_pcall,(void*)0x7401B0);
		replaceCodeWithJump(&luaD_protectedparser,(void*)0x740040);
		replaceCodeWithJump(&luaD_rawrunprotected,(void*)0x73F630);
		replaceCodeWithJump(&luaE_newthread,(void*)0x741F30);
		replaceCodeWithJump(&luaH_new,(void*)0x7428F0);
		replaceCodeWithJump(&luaH_set,(void*)0x742B30);
		replaceCodeWithJump(&luaS_newlstr,(void*)0x742250);
		replaceCodeWithJump(&luaV_execute,(void*)0x7450F0);
		replaceCodeWithJump(&lua_load,(void*)0x73BAE0);
		replaceCodeWithJump(&lua_pcall,(void*)0x73BA70);
		replaceCodeWithJump(&lua_resume,(void*)0x740110);
		replaceCodeWithJump(&lua_setgcthreshold,(void*)0x73BB50);
		replaceCodeWithJump(&lua_yield,(void*)0x73FEF0);
		*/
		// other:
		/*
			
			0x4301A4 calls 0x45FAB0 to update peds
			0x49BC30 is the ped update method (+0x30)
			
			0x43CB16 calls BeginScene
			0x43CEE6 calls EndScene *and* Present
			
			0x560544 setups up view and projection matrix
			
		*/
	}
	return 1;
}
