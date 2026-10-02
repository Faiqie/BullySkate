/* Bully 1.200 native query bridge. Built into a local DSL 9 client.
 * Addresses are verified against the supplied executable by build tooling.
 * No game calls from worker threads; Lua/game thread owns every call. */
#include <dsl/dsl.h>
#include <math.h>
#include <xinput.h>
#include <stdint.h>
#include <stddef.h>
#include "game_compat.h"
#include "controller_input.h"
#include "gameplay_input.h"

typedef struct Vec { float x,y,z; } Vec;
static int readable(const void *p,size_t bytes);
typedef struct Hit { Vec position; void *reference; Vec normal; unsigned char extra[36]; } Hit;
typedef unsigned char (__cdecl *TraceFn)(const Vec*,const Vec*,Hit*,void**,unsigned,int,int);
static int enabled = 0;
static int view_ready=0;
static D3DMATRIX mark_view,mark_projection;
static LARGE_INTEGER board_frequency;
static double board_total_ms=0,board_max_ms=0;
static unsigned board_frames=0,board_ray_count=0;
extern uint32_t __cdecl fs_deadzone(float,float,float,float*);
extern uint32_t __cdecl fs_skate_prepare(void);
extern uint32_t __cdecl fs_skate_error(char*,uint32_t);
extern void* __cdecl fs_skate_mount(float,float,float,float);
extern void __cdecl fs_skate_release(void*);
extern uint32_t __cdecl fs_skate_step(void*,float,uint32_t,float,float,float,float,float,float,float*);
extern uint32_t __cdecl fs_skate_pose(void*,float*);
extern uint32_t __cdecl fs_skate_board_pose(void*,float*);
extern uint32_t __cdecl fs_skate_camera(void*,float,float*);
extern uint32_t __cdecl fs_skate_root(void*,float*);
extern uint32_t __cdecl fs_skate_actors(void*,const float*,uint32_t);
#include "skate_worker.h"
static int rig_active=0;
static float rig_pose[36][13];
static float board_pose[2][13];
static int focused(void) { return GetForegroundWindow()==getGameWindow(); }
static BS_ControllerInput controller_input;
static BS_PadState controller_state;
static wchar_t controller_library[32768];
static int context_pending;
static void poll_controller(void) {
    static int reported=0;
    if(!controller_library[0]) {
        DWORD capacity=(DWORD)(sizeof(controller_library)/sizeof(controller_library[0]));
        DWORD length=GetModuleFileNameW(NULL,controller_library,capacity);
        const wchar_t *relative=L"_derpy_script_loader\\scripts\\BullyMotion\\SDL3.dll";
        wchar_t *slash;
        if(!length||length>=capacity){controller_library[0]=0;return;}
        slash=wcsrchr(controller_library,L'\\');
        if(!slash||capacity-(slash+1-controller_library)<=wcslen(relative)){controller_library[0]=0;return;}
        wcscpy_s(slash+1,capacity-(slash+1-controller_library),relative);
    }
    controller_state=bs_controller_poll(&controller_input,controller_library,focused(),GetTickCount64());
    if(!reported) {
        FILE *file=fopen("_derpy_script_loader/logs/skate-input.log","a");
        if(file){fprintf(file,"PlayStation input: %s\n",controller_input.ready?"ready":"unavailable; using Xbox input");fclose(file);}
        reported=1;
    }
}
/* Called after Bully reads the primary controller, including while paused.
 * Options follows the native pause binding, without sending global keystrokes. */
void fakieUpdateController(void *raw) {
    game_controller *controller=(game_controller*)raw;
    static unsigned previous;
    unsigned pause,held;
    int index=getGamePrimaryControllerIndex();
    static int entered=0;
    if(!entered){FILE *file=fopen("_derpy_script_loader/logs/skate-input.log","a");if(file){fprintf(file,"Controller hook started\n");fclose(file);}entered=1;}
    if(controller==getGameControllers())poll_controller();
    if(index<0||index>3||controller!=getGameControllers()+index)return;
    held=controller_state.connected?controller_state.buttons&16:0;
    if(controller_state.connected) {
        if(controller->is_joy) {
            pause=(unsigned)getGameBindingsBasic()[BS_PAUSE_ACTION];
            controller->input.joystick.buttons&=(short)~pause;
            controller->pressed&=~pause;controller->released&=~pause;
            if(held)controller->input.joystick.buttons|=(short)pause;
            if(held&&!previous)controller->pressed|=pause;
            if(!held&&previous)controller->released|=pause;
        } else if(held) {
            int binding=getGameBindingsAdvanced(0)[BS_PAUSE_ACTION];
            if(binding>=0&&binding<256)controller->input.keyboard[binding]=0x80;
            controller->input.keyboard[DIK_ESCAPE]=0x80;
        }
    }
    previous=held;
    if(context_pending&&!getGamePaused()&&focused()){
        bs_context_action(controller,(unsigned)getGameBindingsBasic()[9],getGameBindingsAdvanced(0)[9]);
        context_pending=0;
    }
}
static unsigned char key_down[256],key_edge[256];static int keys_focused=0;
static void poll_keys(void){
 static const unsigned char watched[]={8,13,27,32,37,38,39,40,65,68,69,82,83,87,116,117,118,119,120,121,160};
 int active=focused(),k;unsigned i;
 for(i=0;i<sizeof(watched);i++){
  k=watched[i];
  SHORT value=GetAsyncKeyState(k);unsigned char down=active&&!!(value&0x8000);
  /* Consume every bound key each frame, including hidden menu keys. This
     prevents old scene input selecting rows, without polling 255 keys. */
  key_edge[k]=active&&keys_focused&&((down&&!key_down[k])||(value&1));key_down[k]=down;
 }
 keys_focused=active;
}

static int compatible(void) {
    return bullyNativeLayoutAvailable();
}
static int FS_Available(lua_State *lua) {
    enabled=compatible(); lua_pushboolean(lua,enabled); return 1;
}
static int query(const Vec *a,const Vec *b,Hit *h,void **hit_entity) {
    void *entity=NULL;
    void *previous_ignore;
    int result=0;
    if (!enabled) return 0;
    memset(h,0,sizeof(*h));
    previous_ignore=*(void**)0xC1AE70;
    __try {
        __try {
            /* The native ray routine tests this exclusion before narrow phase. */
            *(void**)0xC1AE70=*(void**)0xC1AEA8;
            result=((TraceFn)0x461EA0)(a,b,h,&entity,0x4b,0,0);
        } __finally { *(void**)0xC1AE70=previous_ignore; }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { enabled=0; return 0; }
    if (h->reference) {
        ((void(__cdecl*)(void*,void*))0x4657E0)(h->reference,&h->reference);
    }
    *hit_entity=entity;
    return result && isfinite(h->position.x) && isfinite(h->position.y) && isfinite(h->position.z);
}
static int FS_Trace(lua_State *lua) {
    Vec a,b; Hit h; int ok,ped_id=-1,model=-1;void *entity=NULL;
    a.x=luaL_checknumber(lua,1); a.y=luaL_checknumber(lua,2); a.z=luaL_checknumber(lua,3);
    b.x=luaL_checknumber(lua,4); b.y=luaL_checknumber(lua,5); b.z=luaL_checknumber(lua,6);
    ok=query(&a,&b,&h,&entity);
    lua_pushboolean(lua,ok);
    if (!ok) return 1;
    lua_pushnumber(lua,h.position.x); lua_pushnumber(lua,h.position.y); lua_pushnumber(lua,h.position.z);
    lua_pushnumber(lua,h.normal.x); lua_pushnumber(lua,h.normal.y); lua_pushnumber(lua,h.normal.z);
    if(entity) {
        game_pool *pool=getGamePedPool();uintptr_t offset=(uintptr_t)entity-(uintptr_t)pool->array;
        model=*(short*)((char*)entity+0x10E);
        if(pool->size && offset<(uintptr_t)pool->size*pool->limit && offset%pool->size==0 &&
            !(pool->flags[offset/pool->size]&GAME_POOL_INVALID)) ped_id=getGamePedId(entity);
    }
    lua_pushnumber(lua,ped_id);lua_pushnumber(lua,model);return 9;
}
static int FS_Pad(lua_State *lua) {
    poll_keys();
    poll_controller();
    lua_pushboolean(lua,controller_state.connected);
    lua_pushnumber(lua,controller_state.buttons);
    lua_pushnumber(lua,controller_state.lx);lua_pushnumber(lua,controller_state.ly);
    lua_pushnumber(lua,controller_state.rx);lua_pushnumber(lua,controller_state.ry);
    lua_pushnumber(lua,controller_state.lt);lua_pushnumber(lua,controller_state.rt);
    lua_pushnumber(lua,controller_state.kind);return 9;
}
static int FS_Key(lua_State *lua) {
    int key=(int)luaL_checknumber(lua,1),valid=key>0&&key<256&&focused();
    lua_pushboolean(lua,valid&&key_down[key]);lua_pushboolean(lua,valid&&key_edge[key]);return 2;
}
static int FS_Active(lua_State *lua) {
    lua_pushboolean(lua,focused()&&!getGamePaused()&&!getGameMinimized());return 1;
}
/* Source input comes directly from SDL/XInput/Win32. Suppress native movement on
 * the selected controller while keeping Escape/Start available to pause. */
static int FS_MuteGameplay(lua_State *lua){
 int index=getGamePrimaryControllerIndex();
 if(enabled&&index>=0&&index<4) {
  game_controller *controller=getGameControllers()+index;
  bs_mute_gameplay(controller,(unsigned)getGameBindingsBasic()[BS_PAUSE_ACTION],getGameBindingsAdvanced(0)[BS_PAUSE_ACTION]);
 }
 return 0;
}
/* Yield a context action to Bully after releasing our controller filter. */
static int FS_Interact(lua_State *lua){
 if(enabled)context_pending=1;
 return 0;
}
static int FS_BoardPresent(lua_State *lua){
 int present=0;
 if(enabled)__try {
  char *ped=*(char**)0xC1AEA8,*board;
  if(readable(ped,0x1D4)){board=*(char**)(ped+0x1D0);present=readable(board,0x110)&&*(short*)(board+0x10E)==437&&readable(*(void**)(board+0x18),8);}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 lua_pushboolean(lua,present);return 1;
}
/* PedGetPosXYZ returns the collision origin. PedSetPosXYZ adds -bounds.min.z
 * to a requested foot position; use that same model offset for NPC contacts. */
static int FS_PedBaseOffset(lua_State *lua){
 float offset=0.98f,height=1.56f,radius=0.28f;char *ped,*entity;float *bounds;
 if(enabled)__try{
  ped=(char*)getGamePedFromId((int)luaL_checknumber(lua,1),0);
  if(readable(ped,0x1558)){
   entity=*(char**)(ped+0x1554);if(!entity)entity=ped;
   if(readable(entity,0x110)){
    bounds=((float*(__cdecl*)(void*))0x51AF50)(entity);
    if(readable(bounds,44)&&isfinite(bounds[6])&&bounds[6]<=0&&bounds[6]>=-4){
     offset=-bounds[6];
     if(isfinite(bounds[10])&&bounds[10]>bounds[6])height=fminf(2.6f,fmaxf(0.8f,bounds[10]-bounds[6]));
     if(isfinite(bounds[8])&&isfinite(bounds[4])&&isfinite(bounds[9])&&isfinite(bounds[5]))radius=fminf(0.55f,fmaxf(0.18f,0.5f*fmaxf(bounds[8]-bounds[4],bounds[9]-bounds[5])));
    }
   }
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 lua_pushnumber(lua,offset);lua_pushnumber(lua,height);lua_pushnumber(lua,radius);return 3;
}
/* Called from ControllerUpdating during the board equip transition only. */
static int FS_BoardMountInput(lua_State *lua) {
    int index=getGamePrimaryControllerIndex(),binding;
    game_controller *controller;
    if(!enabled||index<0||index>3)return 0;
    controller=getGameControllers()+index;
    if(controller->is_joy)controller->input.joystick.y=0.8f;
    else {
        binding=getGameBindingsAdvanced(0)[17];
        if(binding>=0&&binding<256)controller->input.keyboard[binding]=0x80;
    }
    return 0;
}
static int FS_Deadzone(lua_State *lua) {
    float out[2];fs_deadzone(luaL_checknumber(lua,1),luaL_checknumber(lua,2),luaL_checknumber(lua,3),out);
    lua_pushnumber(lua,out[0]);lua_pushnumber(lua,out[1]);return 2;
}
static int FS_BoardPerf(lua_State *lua) {
    lua_pushnumber(lua,board_frames?board_total_ms/board_frames:0);lua_pushnumber(lua,board_max_ms);
    lua_pushnumber(lua,board_frames);lua_pushnumber(lua,board_ray_count);return 4;
}
static int FS_SkateReady(lua_State *lua){
    char error[1024];unsigned status=fs_skate_prepare();
    lua_pushnumber(lua,status);fs_skate_error(error,sizeof(error));lua_pushstring(lua,error);return 2;
}
static int FS_SkaterOptions(lua_State *lua){
 unsigned k;
 if(lua_gettop(lua)){
  SkaterPreferences next;
  for(k=0;k<11;k++)next.values[k]=(float)luaL_checknumber(lua,k+1);
  if(!skater_preferences_valid(&next))return luaL_error(lua,"Invalid skater settings");
  if(!skater_preferences_save(&next)){lua_pushboolean(lua,0);return 1;}
  skater_preferences=next;lua_pushboolean(lua,1);return 1;
 }
 skater_preferences_load();
 for(k=0;k<11;k++)lua_pushnumber(lua,skater_preferences.values[k]);
 return 11;
}
static int FS_SkateConfigure(lua_State *lua){
 lua_pushboolean(lua,ipc_skate_configure(&skater_preferences));return 1;
}
#include "vehicles.h"
static int FS_SkateFree(lua_State *lua){
    void **host=luaL_checkudata(lua,1,"BullyMotion.Skate");
    rig_active=0;
    if(*host){fs_skate_release(*host);*host=NULL;}return 0;
}
static int FS_SkateNew(lua_State *lua){
    skate_mount_area=(DWORD)luaL_optnumber(lua,5,0);
    void **host=lua_newuserdata(lua,sizeof(void*));
    *host=fs_skate_mount(luaL_checknumber(lua,1),luaL_checknumber(lua,2),luaL_checknumber(lua,3),luaL_checknumber(lua,4));
    if(!*host){lua_pop(lua,1);lua_pushnil(lua);return 1;}
    luaL_getmetatable(lua,"BullyMotion.Skate");lua_setmetatable(lua,-2);return 1;
}
static int FS_SkateStep(lua_State *lua){
    void **host=luaL_checkudata(lua,1,"BullyMotion.Skate");float result[14];
    float dt=luaL_checknumber(lua,2);unsigned buttons=(unsigned)luaL_checknumber(lua,3);int ok;unsigned k;LARGE_INTEGER start,end;
    QueryPerformanceCounter(&start);
    ok=enabled&&*host&&fs_skate_step(*host,dt,buttons,luaL_checknumber(lua,4),luaL_checknumber(lua,5),luaL_checknumber(lua,6),luaL_checknumber(lua,7),luaL_checknumber(lua,8),luaL_checknumber(lua,9),result);
    rig_active=ok&&fs_skate_pose(*host,&rig_pose[0][0])&&fs_skate_board_pose(*host,&board_pose[0][0]);
    QueryPerformanceCounter(&end);
    if(board_frequency.QuadPart){double ms=(end.QuadPart-start.QuadPart)*1000.0/board_frequency.QuadPart;board_total_ms+=ms;if(ms>board_max_ms)board_max_ms=ms;board_frames++;}
    lua_pushboolean(lua,ok);if(!ok)return 1;
    for(k=0;k<14;k++)lua_pushnumber(lua,result[k]);return 15;
}
static int FS_SkateActors(lua_State *lua){
 void **host=luaL_checkudata(lua,1,"BullyMotion.Skate");float rows[24*9];unsigned n,i;
 luaL_checktype(lua,2,LUA_TTABLE);n=(unsigned)luaL_checknumber(lua,3);if(n>24)n=24;
 for(i=0;i<n*9;i++){lua_rawgeti(lua,2,i+1);rows[i]=(float)luaL_checknumber(lua,-1);lua_pop(lua,1);}
 lua_pushboolean(lua,enabled&&*host&&fs_skate_actors(*host,rows,n));return 1;
}

static int FS_SkateCamera(lua_State *lua){
 void **host=luaL_checkudata(lua,1,"BullyMotion.Skate");float out[7];unsigned i;
 int ok=enabled&&*host&&fs_skate_camera(*host,(float)getGameWidth()/getGameHeight(),out);
 lua_pushboolean(lua,ok);if(!ok)return 1;for(i=0;i<7;i++)lua_pushnumber(lua,out[i]);return 8;
}
static int FS_SkateRoot(lua_State *lua){
 void **host=luaL_checkudata(lua,1,"BullyMotion.Skate");float out[3];unsigned i;
 int ok=enabled&&*host&&fs_skate_root(*host,out);lua_pushboolean(lua,ok);if(!ok)return 1;
 for(i=0;i<3;i++)lua_pushnumber(lua,out[i]);return 4;
}
static int FS_SkateMarkerStatus(lua_State *lua){
 lua_pushnumber(lua,skate_snapshot.marker_flags);lua_pushnumber(lua,skate_snapshot.marker_sets);
 lua_pushnumber(lua,skate_snapshot.marker_returns);lua_pushnumber(lua,skate_snapshot.marker_progress);return 4;
}
static int FS_SkateMarkerReset(lua_State *lua){lua_pushboolean(lua,ipc_skate_marker_clear());return 1;}
static int FS_SkateInteraction(lua_State *lua){unsigned i;for(i=0;i<4;i++)lua_pushnumber(lua,skate_snapshot.interaction[i]);return 4;}
/* Read-only rig inspection. Every walk is bounded and every pointer checked. */
static int readable(const void *p,size_t bytes){
    MEMORY_BASIC_INFORMATION region;
    return p&&VirtualQuery(p,&region,sizeof(region))&&region.State==MEM_COMMIT&&!(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
        (uintptr_t)p+bytes<=(uintptr_t)region.BaseAddress+region.RegionSize;
}
static void dump_frame(FILE *file,char *frame,unsigned *count,unsigned depth){
    unsigned k;char *child;
    if(*count>=128||depth>40||!readable(frame,0xA4))return;
    (*count)++;fprintf(file,"frame %p depth %u matrix",frame,depth);
    for(k=0;k<12;k++)fprintf(file," %.5f",((float*)(frame+0x14))[k]);
    fprintf(file," extra");
    for(k=0x90;k<0xA4;k+=4)fprintf(file," %08x",*(unsigned*)(frame+k));
    fprintf(file,"\n");
    for(k=0x90;k<0xA4;k+=4){
        char *candidate=*(char**)(frame+k);unsigned n;
        if(!readable(candidate,64))continue;
        fprintf(file," candidate +%x %p:",k,candidate);
        for(n=0;n<16;n++)fprintf(file," %08x",((unsigned*)candidate)[n]);
        fprintf(file,"\n");
    }
    child=*(char**)(frame+0x84);
    while(readable(child,0x8c)&&*count<128){char *next=*(char**)(child+0x88);dump_frame(file,child,count,depth+1);if(next==child)break;child=next;}
}
static int FS_RigDump(lua_State *lua){
    FILE *file;char *ped=*(char**)0xC1AEA8;unsigned count=0;char *object;
    if(!enabled||!readable(ped,0x118)||fopen_s(&file,"_derpy_script_loader/logs/rig.txt","w")!=0)return 0;
    fprintf(file,"ped %p object18 %p clump114 %p\n",ped,*(void**)(ped+0x18),*(void**)(ped+0x114));
    object=*(char**)(ped+0x18);
    if(readable(object,16)){fprintf(file,"object18 header %08x %08x %08x %08x\n",((unsigned*)object)[0],((unsigned*)object)[1],((unsigned*)object)[2],((unsigned*)object)[3]);dump_frame(file,*(char**)(object+4),&count,0);}
    object=*(char**)(ped+0x114);
    if(readable(object,16)){fprintf(file,"clump114 header %08x %08x %08x %08x\n",((unsigned*)object)[0],((unsigned*)object)[1],((unsigned*)object)[2],((unsigned*)object)[3]);dump_frame(file,*(char**)(object+12),&count,0);}
    fclose(file);lua_pushnumber(lua,count);return 1;
}
#include "rig.h"
/* Publish the root bank after animation; native clips retain Jimmy's skin rig.
 * CEntity::UpdateRW and RwFrameUpdateObjects use the same path as PedFaceHeading. */
static int FS_Bank(lua_State *lua) {
    float yaw=luaL_checknumber(lua,1),pitch=luaL_checknumber(lua,2),roll=luaL_checknumber(lua,3);
    char *ped;float *matrix;Vec right,up,forward;float sy,cy,sp,cp,sr,cr;
    if(!enabled||!isfinite(yaw)||!isfinite(pitch)||!isfinite(roll))return 0;
    pitch=fmaxf(-0.9f,fminf(0.9f,pitch));roll=fmaxf(-0.9f,fminf(0.9f,roll));
    sy=sinf(yaw);cy=cosf(yaw);sp=sinf(pitch);cp=cosf(pitch);sr=sinf(roll);cr=cosf(roll);
    forward=(Vec){-sy*cp,cy*cp,sp};right=(Vec){cy,sy,0};up=(Vec){sy*sp,-cy*sp,cp};
    __try {
        ped=*(char**)0xC1AEA8;if(!ped)return 0;matrix=*(float**)(ped+0x14);if(!matrix)return 0;
        matrix[0]=right.x*cr+up.x*sr;matrix[1]=right.y*cr+up.y*sr;matrix[2]=right.z*cr+up.z*sr;
        matrix[4]=forward.x;matrix[5]=forward.y;matrix[6]=forward.z;
        matrix[8]=up.x*cr-right.x*sr;matrix[9]=up.y*cr-right.y*sr;matrix[10]=up.z*cr-right.z*sr;
        ((void(__fastcall*)(void*,void*))0x443AA0)(ped,NULL);
        if(*(char**)(ped+0x114)){
            void *frame=*(void**)(*(char**)(ped+0x114)+0xC);
            if(frame)((void(__cdecl*)(void*))0x6C9AF0)(frame);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){enabled=0;}
    return 0;
}
static int FS_Cursor(lua_State *lua) {
    POINT p;RECT r;GetCursorPos(&p);ScreenToClient(getGameWindow(),&p);GetClientRect(getGameWindow(),&r);
    lua_pushnumber(lua,(float)p.x/(r.right?r.right:1));lua_pushnumber(lua,(float)p.y/(r.bottom?r.bottom:1));return 2;
}
/* Only our own fixed settings file is writable through this interface. */
static int FS_Options(lua_State *lua) {
    float v[24]={85,180,0.65f,0.18f,0,1,1,2,3,0.0025f,0,4,4,5,1,2,1,1,2,1,1,1,2,1};FILE *f=NULL;int k,save=lua_gettop(lua)>0;
    const char *path="_derpy_script_loader/scripts/BullyMotion/settings.dat";
    if(save) {
        for(k=0;k<24;k++){v[k]=luaL_optnumber(lua,k+1,v[k]);if(!isfinite(v[k]))return luaL_error(lua,"finite settings required");}
        if(fopen_s(&f,path,"wb")==0){fwrite(v,sizeof(float),24,f);fclose(f);}
        lua_pushboolean(lua,f!=NULL);return 1;
    }
    if(fopen_s(&f,path,"rb")==0){float loaded[24];size_t count=fread(loaded,sizeof(float),24,f);if(count>=10){int valid=1;for(k=0;k<(int)count;k++)if(!isfinite(loaded[k]))valid=0;if(valid)memcpy(v,loaded,count*sizeof(float));}fclose(f);}
    for(k=0;k<24;k++)lua_pushnumber(lua,v[k]);return 24;
}

/* Bounded world-space impact marks. Drawn after the world with the game's
 * current view/projection and depth buffer. No allocations in the frame loop. */
#define MAX_MARKS 128
typedef struct Mark {Vec point,normal;float time;} Mark;
typedef struct Vertex {float x,y,z;DWORD color;} Vertex;
#include "viewmodel.h"
typedef struct DrawBackup {
    D3DMATRIX world,view,projection;
    IDirect3DVertexShader9 *vs;
    IDirect3DPixelShader9 *ps;
    IDirect3DVertexDeclaration9 *declaration;
    IDirect3DBaseTexture9 *texture;
    IDirect3DVertexBuffer9 *stream;
    IDirect3DSurface9 *depth;
    UINT stream_offset,stride;
    DWORD render[10],stage[7],sampler[5];
} DrawBackup;
static const D3DRENDERSTATETYPE mark_states[10]={
    D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_CULLMODE,
    D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_ALPHATESTENABLE,
    D3DRS_STENCILENABLE,D3DRS_COLORWRITEENABLE};
static const DWORD mark_values[10]={FALSE,FALSE,FALSE,D3DCULL_NONE,
    TRUE,FALSE,D3DCMP_LESSEQUAL,FALSE,FALSE,15};
static const D3DTEXTURESTAGESTATETYPE mark_stage[7]={D3DTSS_COLOROP,
    D3DTSS_COLORARG1,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_COLOROP,D3DTSS_COLORARG2,D3DTSS_ALPHAARG2};
static const DWORD mark_stage_values[5]={D3DTOP_SELECTARG1,D3DTA_DIFFUSE,
    D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTOP_DISABLE};
static const D3DSAMPLERSTATETYPE view_samplers[5]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV};
static Mark marks[MAX_MARKS];static unsigned next_mark=0;
static float mark_clock=0;
static int FS_View(lua_State *lua) {
    Vec eye,forward,right,up;float length,scale,aspect;float *p=(float*)&eye;unsigned k;
    if(lua_gettop(lua)==0){view_ready=0;return 0;}
    for(k=0;k<3;k++)p[k]=luaL_checknumber(lua,k+1);
    p=(float*)&forward;for(k=0;k<3;k++)p[k]=luaL_checknumber(lua,k+4);
    length=sqrtf(forward.x*forward.x+forward.y*forward.y+forward.z*forward.z);
    if(length<0.1f||!isfinite(length)){view_ready=0;return 0;}
    forward.x/=length;forward.y/=length;forward.z/=length;
    length=sqrtf(forward.x*forward.x+forward.y*forward.y);
    if(length<0.01f){view_ready=0;return 0;}
    right=(Vec){forward.y/length,-forward.x/length,0};
    up=(Vec){right.y*forward.z,-right.x*forward.z,right.x*forward.y-right.y*forward.x};
    memset(&mark_view,0,sizeof(mark_view));
    mark_view._11=right.x;mark_view._21=right.y;mark_view._31=right.z;
    mark_view._12=up.x;mark_view._22=up.y;mark_view._32=up.z;
    mark_view._13=forward.x;mark_view._23=forward.y;mark_view._33=forward.z;
    mark_view._41=-(right.x*eye.x+right.y*eye.y+right.z*eye.z);
    mark_view._42=-(up.x*eye.x+up.y*eye.y+up.z*eye.z);
    mark_view._43=-(forward.x*eye.x+forward.y*eye.y+forward.z*eye.z);
    mark_view._44=1;
    scale=1.0f/tanf((float)luaL_checknumber(lua,7)*0.00872664626f);
    aspect=(float)getGameWidth()/getGameHeight();
    memset(&mark_projection,0,sizeof(mark_projection));
    mark_projection._11=scale/aspect;mark_projection._22=scale;
    mark_projection._33=1000.0f/(1000.0f-0.1f);mark_projection._34=1;
    mark_projection._43=-0.1f*mark_projection._33;view_ready=1;return 0;
}
static int FS_Mark(lua_State *lua) {
    Mark *m=&marks[next_mark++%MAX_MARKS];float *p=(float*)&m->point;
    unsigned k;float length;
    for(k=0;k<6;k++)p[k]=(float)luaL_checknumber(lua,1+k);
    length=sqrtf(m->normal.x*m->normal.x+m->normal.y*m->normal.y+m->normal.z*m->normal.z);
    if(length<0.1f||!isfinite(length)){m->time=-1000;return 0;}
    m->normal.x/=length;m->normal.y/=length;m->normal.z/=length;m->time=getGameTimer()/1000.0f;
    return 0;
}
static int FS_Clear(lua_State *lua) {unsigned k;for(k=0;k<MAX_MARKS;k++)marks[k].time=-1000;next_mark=0;return 0;}
void fakieDrawWorld(void *arg) {
    dsl_state *dsl=(dsl_state*)arg;IDirect3DDevice9 *d;DrawBackup backup;
    Vertex vertices[MAX_MARKS*8*3];unsigned count=0,k,j;float clock;D3DMATRIX world;
    if(!dsl||!dsl->render||!enabled||(!next_mark&&!weapon_enabled))return;
    clock=getGameTimer()/1000.0f;mark_clock=clock;
    for(k=0;k<MAX_MARKS;k++) {
        Mark *m=&marks[k];Vec u,v,c;float l,age=clock-m->time;
        if(age<0||age>90)continue;
        u.x=-m->normal.y;u.y=m->normal.x;u.z=0;
        l=sqrtf(u.x*u.x+u.y*u.y);
        if(l<0.01f){u.x=1;u.y=0;u.z=0;}else{u.x/=l;u.y/=l;}
        v.x=m->normal.y*u.z-m->normal.z*u.y;
        v.y=m->normal.z*u.x-m->normal.x*u.z;
        v.z=m->normal.x*u.y-m->normal.y*u.x;
        c.x=m->point.x+m->normal.x*0.012f;c.y=m->point.y+m->normal.y*0.012f;c.z=m->point.z+m->normal.z*0.012f;
        for(j=0;j<8;j++) {
            float a=j*0.785398163f,b=(j+1)*0.785398163f,r=0.038f;
            Vertex *t=vertices+count;count+=3;
            t[0]=(Vertex){c.x,c.y,c.z,0xff090807};
            t[1]=(Vertex){c.x+(u.x*cosf(a)+v.x*sinf(a))*r,c.y+(u.y*cosf(a)+v.y*sinf(a))*r,c.z+(u.z*cosf(a)+v.z*sinf(a))*r,0xff302a23};
            t[2]=(Vertex){c.x+(u.x*cosf(b)+v.x*sinf(b))*r,c.y+(u.y*cosf(b)+v.y*sinf(b))*r,c.z+(u.z*cosf(b)+v.z*sinf(b))*r,0xff302a23};
        }
    }
    if(!count&&!weapon_enabled)return;d=getRenderDevice(dsl->render);
    /* Save only states we modify. DrawPrimitiveUP also clears stream zero.
     * All storage is on the stack; COM getters only retain existing objects. */
    memset(&backup,0,sizeof(backup));
    IDirect3DDevice9_GetTransform(d,D3DTS_WORLD,&backup.world);
    IDirect3DDevice9_GetTransform(d,D3DTS_VIEW,&backup.view);
    IDirect3DDevice9_GetTransform(d,D3DTS_PROJECTION,&backup.projection);
    IDirect3DDevice9_GetVertexShader(d,&backup.vs);
    IDirect3DDevice9_GetPixelShader(d,&backup.ps);
    IDirect3DDevice9_GetVertexDeclaration(d,&backup.declaration);
    IDirect3DDevice9_GetTexture(d,0,&backup.texture);
    IDirect3DDevice9_GetStreamSource(d,0,&backup.stream,&backup.stream_offset,&backup.stride);
    IDirect3DDevice9_GetDepthStencilSurface(d,&backup.depth);
    for(k=0;k<10;k++)IDirect3DDevice9_GetRenderState(d,mark_states[k],backup.render+k);
    for(k=0;k<7;k++)IDirect3DDevice9_GetTextureStageState(d,k==4?1:0,mark_stage[k],backup.stage+k);
    for(k=0;k<5;k++)IDirect3DDevice9_GetSamplerState(d,0,view_samplers[k],backup.sampler+k);
    memset(&world,0,sizeof(world));world._11=world._22=world._33=world._44=1;
    IDirect3DDevice9_SetTransform(d,D3DTS_WORLD,&world);
    if(view_ready){
        IDirect3DDevice9_SetTransform(d,D3DTS_VIEW,&mark_view);
        IDirect3DDevice9_SetTransform(d,D3DTS_PROJECTION,&mark_projection);
    }
    IDirect3DDevice9_SetVertexShader(d,NULL);IDirect3DDevice9_SetPixelShader(d,NULL);
    IDirect3DDevice9_SetFVF(d,D3DFVF_XYZ|D3DFVF_DIFFUSE);
    IDirect3DDevice9_SetTexture(d,0,NULL);
    for(k=0;k<10;k++)IDirect3DDevice9_SetRenderState(d,mark_states[k],mark_values[k]);
    for(k=0;k<5;k++)IDirect3DDevice9_SetTextureStageState(d,k==4?1:0,mark_stage[k],mark_stage_values[k]);
    if(count)IDirect3DDevice9_DrawPrimitiveUP(d,D3DPT_TRIANGLELIST,count/3,vertices,sizeof(Vertex));
    for(k=0;k<5;k++)IDirect3DDevice9_SetSamplerState(d,0,view_samplers[k],k<3?D3DTEXF_LINEAR:D3DTADDRESS_WRAP);
    weapon_draw(d,backup.depth,clock);
    IDirect3DDevice9_SetDepthStencilSurface(d,backup.depth);
    IDirect3DDevice9_SetTransform(d,D3DTS_WORLD,&backup.world);
    IDirect3DDevice9_SetTransform(d,D3DTS_VIEW,&backup.view);
    IDirect3DDevice9_SetTransform(d,D3DTS_PROJECTION,&backup.projection);
    IDirect3DDevice9_SetVertexShader(d,backup.vs);
    IDirect3DDevice9_SetPixelShader(d,backup.ps);
    IDirect3DDevice9_SetVertexDeclaration(d,backup.declaration);
    IDirect3DDevice9_SetTexture(d,0,backup.texture);
    IDirect3DDevice9_SetStreamSource(d,0,backup.stream,backup.stream_offset,backup.stride);
    for(k=0;k<10;k++)IDirect3DDevice9_SetRenderState(d,mark_states[k],backup.render[k]);
    for(k=0;k<7;k++)IDirect3DDevice9_SetTextureStageState(d,k==4?1:0,mark_stage[k],backup.stage[k]);
    for(k=0;k<5;k++)IDirect3DDevice9_SetSamplerState(d,0,view_samplers[k],backup.sampler[k]);
    if(backup.vs)IDirect3DVertexShader9_Release(backup.vs);
    if(backup.ps)IDirect3DPixelShader9_Release(backup.ps);
    if(backup.declaration)IDirect3DVertexDeclaration9_Release(backup.declaration);
    if(backup.texture)IDirect3DBaseTexture9_Release(backup.texture);
    if(backup.stream)IDirect3DVertexBuffer9_Release(backup.stream);
    if(backup.depth)IDirect3DSurface9_Release(backup.depth);
}
int dslopen_fakie(lua_State *lua) {
    QueryPerformanceFrequency(&board_frequency);
    luaL_newmetatable(lua,"BullyMotion.Skate");lua_pushstring(lua,"__gc");lua_pushcfunction(lua,FS_SkateFree);lua_settable(lua,-3);lua_pop(lua,1);
    lua_register(lua,"FS_SkateReady",FS_SkateReady);lua_register(lua,"FS_SkateNew",FS_SkateNew);
    lua_register(lua,"FS_SkaterOptions",FS_SkaterOptions);lua_register(lua,"FS_SkateConfigure",FS_SkateConfigure);
    lua_register(lua,"FS_SkateStep",FS_SkateStep);lua_register(lua,"FS_SkateFree",FS_SkateFree);
    lua_register(lua,"FS_SkateCamera",FS_SkateCamera);lua_register(lua,"FS_SkateRoot",FS_SkateRoot);
    lua_register(lua,"FS_SkateMarkerStatus",FS_SkateMarkerStatus);lua_register(lua,"FS_SkateMarkerReset",FS_SkateMarkerReset);
    lua_register(lua,"FS_SkateActors",FS_SkateActors);
    lua_register(lua,"FS_SkateVehicles",FS_SkateVehicles);lua_register(lua,"FS_SkateInteraction",FS_SkateInteraction);
    
    lua_register(lua,"FS_RigDump",FS_RigDump);
    lua_register(lua,"FS_RigStatus",FS_RigStatus);
    lua_register(lua,"FS_Available",FS_Available);
    lua_register(lua,"FS_Trace",FS_Trace);
    lua_register(lua,"FS_Pad",FS_Pad);
    lua_register(lua,"FS_Key",FS_Key);lua_register(lua,"FS_Active",FS_Active);
    lua_register(lua,"FS_MuteGameplay",FS_MuteGameplay);
    lua_register(lua,"FS_Interact",FS_Interact);lua_register(lua,"FS_BoardPresent",FS_BoardPresent);
    lua_register(lua,"FS_PedBaseOffset",FS_PedBaseOffset);
    lua_register(lua,"FS_BoardMountInput",FS_BoardMountInput);
    
    
    lua_register(lua,"FS_Deadzone",FS_Deadzone);
    lua_register(lua,"FS_Mark",FS_Mark);lua_register(lua,"FS_Clear",FS_Clear);
    lua_register(lua,"FS_View",FS_View);
    lua_register(lua,"FS_Weapon",FS_Weapon);
    
    lua_register(lua,"FS_Bank",FS_Bank);lua_register(lua,"FS_BoardPerf",FS_BoardPerf);
    lua_register(lua,"FS_Cursor",FS_Cursor);lua_register(lua,"FS_Options",FS_Options);
    return 0;
}
