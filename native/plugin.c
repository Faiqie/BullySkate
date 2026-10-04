/* BullySkate is an independent ASI. DSL owns scripts, configuration and drawing.
 * Chain existing game callbacks after ASIs have loaded; never replace DSL. */
#include <dsl/dsl.h>
#include "game_compat.h"
#include <math.h>
void luaC_collectgarbage(void);
int dslopen_fakie(lua_State*);
void fakieUpdateController(void*);
void fakieBeforeWorld(void*);
void fakieAfterWorld(void);
static LONG initialized,installed;
static BOOL (WINAPI *system_parameters)(UINT,UINT,PVOID,UINT);
static void (__fastcall *init_lua)(void*,void*);
static void (__cdecl *update_controller)(void*);
static void (__cdecl *draw_world)(void);
static void (__cdecl *update_system)(void);
static unsigned char (__cdecl *begin_scene)(void*,int);
static WNDPROC window_proc;
static HWND hooked_window;
static void log_plugin(const char *message){
 FILE *f=fopen("_derpy_script_loader/logs/bullyskate-plugin.log","a");
 if(f){fprintf(f,"%s\n",message);fclose(f);}
}
static int patch(void *where,const void *data,size_t count){
 DWORD previous,unused;
 if(!VirtualProtect(where,count,PAGE_EXECUTE_READWRITE,&previous))return 0;
 memcpy(where,data,count);FlushInstructionCache(GetCurrentProcess(),where,count);
 VirtualProtect(where,count,previous,&unused);return 1;
}
static void *call_target(DWORD site){
 unsigned char *p=(unsigned char*)site;MEMORY_BASIC_INFORMATION info;void *target;
 if(!bullyMappedRange(site,5,1,0))return NULL;
 if(p[0]==0xE8)target=p+5+*(int32_t*)(p+1);
 else if(p[0]==0xFF&&p[1]==0x15&&bullyMappedRange(site,6,1,0)){
  DWORD slot=*(DWORD*)(p+2);if(!bullyMappedRange(slot,4,0,0))return NULL;target=*(void**)slot;
 }else return NULL;
 if(!VirtualQuery(target,&info,sizeof(info))||info.State!=MEM_COMMIT||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return NULL;
 return target;
}
static int replace_call(DWORD site,void *target){
 unsigned char code[6]={0xE8,0,0,0,0,0x90};size_t length=*(unsigned char*)site==0xFF?6:5;
 *(int32_t*)(code+1)=(char*)target-((char*)site+5);
 return patch((void*)site,code,length);
}
static int jump(void *from,void *to){
 unsigned char code[5]={0xE9};*(int32_t*)(code+1)=(char*)to-((char*)from+5);
 return patch(from,code,5);
}
#include "performance.h"
#include "video.h"
static LRESULT CALLBACK close_window(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_CLOSE||(m==WM_SYSCOMMAND&&(a&0xFFF0)==SC_CLOSE)||
    (m==WM_SYSKEYDOWN&&a==VK_F4&&(b&(1L<<29)))){ExitProcess(0);return 0;}
 return CallWindowProc(window_proc,w,m,a,b);
}
static void __cdecl system_tick(void){
 HWND w=getGameWindow();
 if(w&&hooked_window!=w){window_proc=(WNDPROC)SetWindowLongPtr(w,GWLP_WNDPROC,(LONG_PTR)close_window);hooked_window=w;}
 if(w&&GetForegroundWindow()==w&&(GetAsyncKeyState(VK_MENU)&0x8000)&&(GetAsyncKeyState(VK_F4)&0x8000))ExitProcess(0);
 update_system();
}
static void __fastcall lua_ready(void *game,void *unused){
 init_lua(game,unused);dslopen_fakie(getGameLuaState(game));
 log_plugin("Skating API registered in game Lua; official DSL retains script loading");
}
static void __cdecl controller_ready(void *controller){update_controller(controller);fakieUpdateController(controller);}
static unsigned char __cdecl world_begin(void *camera,int flags){
 unsigned char result=begin_scene(camera,flags);if(result){video_world_active=1;video_detail_begin();video_filter();fakieBeforeWorld((void*)1);}return result;
}
static void __cdecl world_end(void){draw_world();video_world_active=0;video_detail_end();fakieAfterWorld();}
static int interface_verified(void){
 /* These are the native interfaces used by the skating bridge. DSL is allowed
  * to redirect call sites, so the old loader's own hook sites are not probes. */
 static const DWORD required[]={0x443AA0,0x461EA0,0x4657E0,0x454CD0,0x51AF50,0x5C83B0,0x6C9AF0};
 unsigned i,j;if(!bullyNativeLayoutAvailable())return 0;
 for(i=0;i<sizeof(required)/sizeof(required[0]);i++){
  for(j=0;j<sizeof(bullyCodeProbes)/sizeof(bullyCodeProbes[0]);j++)if(bullyCodeProbes[j].address==required[i])break;
  if(j==sizeof(bullyCodeProbes)/sizeof(bullyCodeProbes[0])||
     !bullyFingerprintMatches((void*)required[i],bullyCodeProbes[j].length,bullyCodeProbes[j].hash))return 0;
 }
 for(i=0;i<sizeof(bullyDataProbes)/sizeof(bullyDataProbes[0]);i++)
  if(!bullyMappedRange(bullyDataProbes[i].address,bullyDataProbes[i].length,0,bullyDataProbes[i].writable))return 0;
 return 1;
}
static void install_hooks(void){
 if(InterlockedCompareExchange(&installed,1,0))return;
 if(!interface_verified()){log_plugin("Unsupported native interface; no game hooks installed");return;}
 init_lua=(void*)call_target(0x5DC374);update_controller=(void*)call_target(0x738617);
 draw_world=(void*)call_target(0x43CCA6);update_system=(void*)call_target(0x43D650);begin_scene=(void*)call_target(0x43CB16);
 if(!init_lua||!update_controller||!draw_world||!update_system||!begin_scene){log_plugin("Unsupported callback layout; no game hooks installed");return;}
 /* Lua's game ABI uses float numbers and shared GC/thread creation. */
 if(!jump(lua_close,(void*)0x7420B0)||!jump(lua_newthread,(void*)0x73AE60)||
    !jump(lua_open,(void*)0x742010)||!jump(luaC_collectgarbage,(void*)0x740F20))return;
 if(!replace_call(0x5DC374,lua_ready)||!replace_call(0x738617,controller_ready)||
    !replace_call(0x43CCA6,world_end)||!replace_call(0x43D650,system_tick)||!replace_call(0x43CB16,world_begin))return;
 performance_load();video_install();
 log_plugin("Verified native interfaces; chained existing DSL/render/controller callbacks");
}
static void apply_launch_settings(void){
 char setting[32]={0};FILE *hint=fopen("_derpy_script_loader/bullyskate-launch.txt","rb");
 if(hint){fread(setting,1,sizeof(setting)-1,hint);fclose(hint);remove("_derpy_script_loader/bullyskate-launch.txt");
  if(strstr(setting,"display=0"))SetEnvironmentVariableA("BULLY_SKATE_DISPLAY","0");
  else if(strstr(setting,"display=1"))SetEnvironmentVariableA("BULLY_SKATE_DISPLAY","1");
  if(!strncmp(setting,"dpi-aware\n",10)){
   typedef BOOL (WINAPI *Aware)(void);Aware aware=(Aware)GetProcAddress(GetModuleHandleA("user32.dll"),"SetProcessDPIAware");
   if(aware)aware();
  }
 }
}
static BOOL WINAPI before_system(UINT a,UINT b,PVOID c,UINT d){install_hooks();return system_parameters(a,b,c,d);}
static int hook_system_import(void){
 char *base=(char*)GetModuleHandle(NULL);IMAGE_DOS_HEADER *dos=(void*)base;IMAGE_NT_HEADERS32 *nt;IMAGE_IMPORT_DESCRIPTOR *imports;
 if(!bullyNativeLayoutAvailable())return 0;
 nt=(void*)(base+dos->e_lfanew);imports=(void*)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
 for(;imports->Name;imports++)if(!_stricmp(base+imports->Name,"user32.dll")){
  IMAGE_THUNK_DATA32 *names,*slots;
  if(!imports->OriginalFirstThunk)return 0;
  names=(void*)(base+imports->OriginalFirstThunk);slots=(void*)(base+imports->FirstThunk);
  for(;names->u1.AddressOfData;names++,slots++)if(!(names->u1.Ordinal&IMAGE_ORDINAL_FLAG32)){
   IMAGE_IMPORT_BY_NAME *name=(void*)(base+names->u1.AddressOfData);
   if(!strcmp((char*)name->Name,"SystemParametersInfoA")){
    void *replacement=before_system;system_parameters=(void*)slots->u1.Function;return patch(&slots->u1.Function,&replacement,4);
   }
  }
 }
 return 0;
}
#pragma comment(linker,"/EXPORT:InitializeASI=_InitializeASI")
void __cdecl InitializeASI(void){
 if(InterlockedCompareExchange(&initialized,1,0))return;
 if(!hook_system_import()){initialized=0;log_plugin("Unsupported host; no hooks installed");}
 else apply_launch_settings();
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);InitializeASI();}
 return TRUE;
}
