#include <dsl/dsl.h>
#include <math.h>
#include <assert.h>
static void log_plugin(const char *message){(void)message;}
static int patch(void *at,const void *data,size_t size){memcpy(at,data,size);return 1;}
static void *call_target(DWORD site){(void)site;return NULL;}
static int replace_call(DWORD site,void *function){(void)site;(void)function;return 0;}
#include "../native/performance.h"
#include "../native/video.h"
static D3DPRESENT_PARAMETERS seen[4];static int calls,fail_first;
static void *table[119];static void **device=table;
static HRESULT __stdcall reset_stub(IDirect3DDevice9 *d,D3DPRESENT_PARAMETERS *p){(void)d;seen[calls++]=*p;return fail_first&&calls==1?D3DERR_INVALIDCALL:D3D_OK;}
static HRESULT __stdcall present_stub(IDirect3DDevice9 *d,const RECT *a,const RECT *b,HWND w,const RGNDATA *r){(void)d;(void)a;(void)b;(void)w;(void)r;return D3D_OK;}
static HRESULT __stdcall create_stub(void *a,UINT b,D3DDEVTYPE c,HWND d,DWORD e,D3DPRESENT_PARAMETERS *p,IDirect3DDevice9 **out){(void)a;(void)b;(void)c;(void)d;(void)e;seen[calls++]=*p;if(fail_first&&calls==1)return D3DERR_INVALIDCALL;*out=(void*)&device;return D3D_OK;}
static DWORD mip[8];static unsigned sampler_calls;
static HRESULT __stdcall get_sampler_stub(IDirect3DDevice9 *d,DWORD sampler,D3DSAMPLERSTATETYPE state,DWORD *value){(void)d;assert(sampler<8&&state==D3DSAMP_MIPMAPLODBIAS);*value=mip[sampler];return D3D_OK;}
static HRESULT __stdcall set_sampler_stub(IDirect3DDevice9 *d,DWORD sampler,D3DSAMPLERSTATETYPE state,DWORD value){(void)d;assert(sampler<8&&state==D3DSAMP_MIPMAPLODBIAS);mip[sampler]=value;sampler_calls++;return D3D_OK;}
int main(void){
 D3DPRESENT_PARAMETERS original={0},p;IDirect3DDevice9 *out;int mode,vsync;
 original.BackBufferWidth=1920;original.BackBufferHeight=1080;original.BackBufferFormat=D3DFMT_A8R8G8B8;original.SwapEffect=D3DSWAPEFFECT_DISCARD;original.EnableAutoDepthStencil=TRUE;original.AutoDepthStencilFormat=D3DFMT_D24S8;original.FullScreen_RefreshRateInHz=60;original.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
 QueryPerformanceFrequency(&video_frequency);
 for(mode=0;mode<3;mode++)for(vsync=0;vsync<3;vsync++){
  video_applied=(VideoSettings){mode,vsync,0,0,100,0};p=original;calls=0;fail_first=0;table[16]=reset_stub;table[17]=present_stub;video_create=create_stub;
  assert(video_create_device(NULL,0,D3DDEVTYPE_HAL,NULL,0,&p,&out)==D3D_OK);assert(calls==1);
  assert(p.Windowed==(mode!=0));assert(p.FullScreen_RefreshRateInHz==(mode?0:60));assert(p.PresentationInterval==(vsync==1?D3DPRESENT_INTERVAL_IMMEDIATE:D3DPRESENT_INTERVAL_ONE));
  assert(p.BackBufferWidth==1920&&p.BackBufferHeight==1080&&p.EnableAutoDepthStencil&&p.AutoDepthStencilFormat==D3DFMT_D24S8&&p.BackBufferFormat==original.BackBufferFormat);
  assert(video_present_frame(out,NULL,NULL,NULL,NULL)==D3D_OK);
 }
 video_applied=(VideoSettings){1,1,0,0,100,0};calls=0;fail_first=1;p=original;table[16]=reset_stub;table[17]=present_stub;
 assert(video_create_device(NULL,0,D3DDEVTYPE_HAL,NULL,0,&p,&out)==D3D_OK);assert(calls==2&&memcmp(&seen[1],&original,sizeof(original))==0);assert(video_applied.mode==0);
 video_applied.mode=1;calls=0;fail_first=1;p=original;video_reset=reset_stub;assert(video_reset_device(out,&p)==D3D_OK);assert(calls==2&&memcmp(&p,&original,sizeof(p))==0);
 assert(video_valid(&(VideoSettings){2,2,6,4,130,1}));assert(!video_valid(&(VideoSettings){3,0,0,0,100,0}));assert(!video_valid(&(VideoSettings){0,0,0,0,99,0}));
 assert(performance_valid(&performance_defaults));assert(!performance_valid(&(PerformanceSettings){0,0,0,0,2,0,0,0,0}));
 performance.texture=2;{DWORD bias=video_detail_bias();float f;memcpy(&f,&bias,4);assert(f==2.0f);}performance=performance_defaults;
 assert(bs_traffic_budget()==8&&bs_traffic_range()==35);performance.cars=0;performance.car_range=0;assert(bs_traffic_budget()==4&&bs_traffic_range()==15);
 {
  unsigned i;DWORD original_bias[8];table[68]=get_sampler_stub;table[69]=video_set_sampler;video_sampler=set_sampler_stub;
  for(i=0;i<8;i++){float f=(float)i*.1f;memcpy(&mip[i],&f,4);original_bias[i]=mip[i];}
  performance.texture=0;sampler_calls=0;video_detail_begin();assert(sampler_calls==0);
  performance.texture=2;video_world_active=1;video_detail_begin();assert(sampler_calls==8);
  for(i=0;i<8;i++)assert(mip[i]==video_detail_bias());
  video_set_sampler(out,0,D3DSAMP_MIPMAPLODBIAS,0);assert(mip[0]==video_detail_bias());
  video_world_active=0;video_detail_end();for(i=0;i<8;i++)assert(mip[i]==original_bias[i]);
  video_set_sampler(out,0,D3DSAMP_MIPMAPLODBIAS,0);assert(mip[0]==0);
 }
 {
  RECT monitor={1920,-2160,5760,0},outer=monitor;
  assert(!video_borderless_mismatch(WS_POPUP|WS_VISIBLE|WS_SYSMENU,0,&outer,&monitor));
  assert(video_borderless_mismatch(WS_OVERLAPPEDWINDOW,0,&outer,&monitor));
  assert(video_borderless_mismatch(WS_POPUP,WS_EX_CLIENTEDGE,&outer,&monitor));
  outer.right=3840;assert(video_borderless_mismatch(WS_POPUP,0,&outer,&monitor));
  outer=monitor;outer.top=0;assert(video_borderless_mismatch(WS_POPUP,0,&outer,&monitor));
 }
 puts("PASS: presentation modes, fallback, 4K multi-monitor borderless repair, live performance limits, world mip overrides and HUD sampler restoration.");return 0;
}
