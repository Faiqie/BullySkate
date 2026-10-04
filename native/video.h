/* Keep the native render size. Windowed Present scales the complete image to
 * the client area; changing display mode/vsync waits for the next game start. */
typedef struct VideoSettings {DWORD mode,vsync,filter,cap,scale,fps;} VideoSettings;
static VideoSettings video={0,0,0,0,100,0},video_applied;
static IDirect3DDevice9 *video_device;
static HRESULT (__stdcall *video_create)(void*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
static HRESULT (__stdcall *video_reset)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
static HRESULT (__stdcall *video_present)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
static LONG (WINAPI *video_display_change)(DEVMODEA*,DWORD);
static HRESULT (__stdcall *video_sampler)(IDirect3DDevice9*,DWORD,D3DSAMPLERSTATETYPE,DWORD);
static int video_world_active;
static D3DCAPS9 video_caps;static int video_has_caps;
static HRESULT __stdcall video_set_sampler(IDirect3DDevice9*,DWORD,D3DSAMPLERSTATETYPE,DWORD);
static int video_available;static unsigned video_width,video_height;static float video_fps;
static DWORD video_mip_saved[8];static int video_mip_active;
static LARGE_INTEGER video_frequency,video_last;static RECT video_monitor;
static HWND video_window;
static ULONGLONG video_style_check;
static const char *video_path="_derpy_script_loader/scripts/BullyMotion/video-settings.dat";
static int video_valid(const VideoSettings *v){return v->mode<=2&&v->vsync<=2&&v->filter<=6&&v->cap<=4&&v->scale>=80&&v->scale<=130&&v->scale%5==0&&v->fps<=1;}
static int video_save(const VideoSettings *v){
 FILE *f;DWORD version=1;int valid;const char *temp="_derpy_script_loader/scripts/BullyMotion/video-settings.dat.new";
 if(fopen_s(&f,temp,"wb"))return 0;
 valid=fwrite(&version,4,1,f)==1&&fwrite(v,sizeof(*v),1,f)==1;
 if(fflush(f))valid=0;if(fclose(f))valid=0;
 return valid&&MoveFileExA(temp,video_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
static void video_load(void){
 FILE *f;DWORD version;VideoSettings loaded;char override[8];
 if(!fopen_s(&f,video_path,"rb")){
  int valid=fread(&version,4,1,f)==1&&version==1&&fread(&loaded,sizeof(loaded),1,f)==1&&fgetc(f)==EOF&&video_valid(&loaded);
  fclose(f);if(valid)video=loaded;
 }
 if(GetEnvironmentVariableA("BULLY_SKATE_DISPLAY",override,sizeof(override))){if(override[0]=='0')video.mode=0;else if(override[0]=='1')video.mode=1;}
 else if(GetEnvironmentVariableA("BULLY_SKATE_WINDOWED",override,sizeof(override))&&override[0]=='1')video.mode=1;
 video_applied=video;QueryPerformanceFrequency(&video_frequency);
}
static void video_parameters(D3DPRESENT_PARAMETERS *p){
 if(video_applied.mode){p->Windowed=TRUE;p->FullScreen_RefreshRateInHz=0;}
 if(video_applied.vsync)p->PresentationInterval=video_applied.vsync==1?D3DPRESENT_INTERVAL_IMMEDIATE:D3DPRESENT_INTERVAL_ONE;
}
static LONG WINAPI video_change_display(DEVMODEA *mode,DWORD flags){
 /* Borderless uses the existing desktop mode, including other monitors. */
 if(video_applied.mode)return DISP_CHANGE_SUCCESSFUL;
 return video_display_change(mode,flags);
}
static void video_window_style(HWND window){
 MONITORINFO monitor={sizeof(monitor)};RECT client;DWORD style;
 if(!window||!video_applied.mode)return;
 if(!GetMonitorInfo(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return;
 video_monitor=monitor.rcMonitor;
 style=video_applied.mode==1?(WS_POPUP|WS_VISIBLE|WS_SYSMENU):(WS_OVERLAPPEDWINDOW|WS_VISIBLE);
 client=(RECT){0,0,(LONG)video_width,(LONG)video_height};
 if(video_applied.mode==1)client=monitor.rcMonitor;
 else {int w,h;AdjustWindowRect(&client,style,FALSE);w=client.right-client.left;h=client.bottom-client.top;
  if(w>monitor.rcWork.right-monitor.rcWork.left)w=monitor.rcWork.right-monitor.rcWork.left;
  if(h>monitor.rcWork.bottom-monitor.rcWork.top)h=monitor.rcWork.bottom-monitor.rcWork.top;
  client.left=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-w)/2;
  client.top=monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-h)/2;client.right=client.left+w;client.bottom=client.top+h;
 }
 SetWindowLongPtr(window,GWL_STYLE,style);
 if(video_applied.mode==1){LONG_PTR extended=GetWindowLongPtr(window,GWL_EXSTYLE);extended&=~(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_DLGMODALFRAME|WS_EX_STATICEDGE);SetWindowLongPtr(window,GWL_EXSTYLE,extended);}
 SetWindowPos(window,NULL,client.left,client.top,client.right-client.left,client.bottom-client.top,SWP_NOZORDER|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
}
static int video_borderless_mismatch(LONG_PTR style,LONG_PTR extended,const RECT *outer,const RECT *monitor){
 return (style&(WS_CAPTION|WS_THICKFRAME))||(extended&(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_DLGMODALFRAME|WS_EX_STATICEDGE))||
    outer->left!=monitor->left||outer->top!=monitor->top||outer->right!=monitor->right||outer->bottom!=monitor->bottom;
}
static void video_keep_window_style(void){
 RECT outer;MONITORINFO monitor={sizeof(monitor)};ULONGLONG now=GetTickCount64();LONG_PTR style,extended;
 if(!video_window||video_applied.mode!=1||now<video_style_check)return;
 video_style_check=now+250;
 if(IsIconic(video_window)||!GetWindowRect(video_window,&outer)||!GetMonitorInfo(MonitorFromWindow(video_window,MONITOR_DEFAULTTONEAREST),&monitor))return;
 style=GetWindowLongPtr(video_window,GWL_STYLE);extended=GetWindowLongPtr(video_window,GWL_EXSTYLE);
 /* Bully and widescreen plugins resize/restyle after CreateDevice. Repair only
    a real mismatch, after their initialization, without resetting D3D. */
 if(video_borderless_mismatch(style,extended,&outer,&monitor.rcMonitor)){
  video_window_style(video_window);log_plugin("Borderless window restored after native window setup");
 }
}
static HRESULT __stdcall video_reset_device(IDirect3DDevice9 *device,D3DPRESENT_PARAMETERS *p){
 HRESULT hr;D3DPRESENT_PARAMETERS requested=*p;video_parameters(p);hr=video_reset(device,p);
 if(FAILED(hr)){*p=requested;hr=video_reset(device,p);if(SUCCEEDED(hr)){video_applied.mode=requested.Windowed?2:0;video_applied.vsync=0;}}
 if(SUCCEEDED(hr)){video_width=p->BackBufferWidth;video_height=p->BackBufferHeight;video_window_style(video_window);}
 return hr;
}
static HRESULT __stdcall video_present_frame(IDirect3DDevice9 *device,const RECT *source,const RECT *dest,HWND window,const RGNDATA *dirty){
 static const DWORD caps[]={0,30,60,90,120};LARGE_INTEGER now;double elapsed;DWORD cap=caps[video.cap];
 video_keep_window_style();QueryPerformanceCounter(&now);
 if(video_last.QuadPart){elapsed=(double)(now.QuadPart-video_last.QuadPart)/video_frequency.QuadPart;
  if(cap&&GetForegroundWindow()==video_window){double remaining=1.0/cap-elapsed;if(remaining>0.001){Sleep((DWORD)(remaining*1000));QueryPerformanceCounter(&now);elapsed=(double)(now.QuadPart-video_last.QuadPart)/video_frequency.QuadPart;}}
  if(elapsed>0&&elapsed<1)video_fps=video_fps>0?video_fps*0.9f+(float)(0.1/elapsed):(float)(1.0/elapsed);
 }
 video_last=now;return video_present(device,source,dest,window,dirty);
}
static HRESULT __stdcall video_create_device(void *d3d,UINT adapter,D3DDEVTYPE type,HWND focus,DWORD behavior,D3DPRESENT_PARAMETERS *p,IDirect3DDevice9 **device){
 HRESULT hr;D3DPRESENT_PARAMETERS original=*p;video_parameters(p);
 hr=video_create(d3d,adapter,type,focus,behavior,p,device);
 if(FAILED(hr)){*p=original;video_applied.mode=original.Windowed?2:0;video_applied.vsync=0;hr=video_create(d3d,adapter,type,focus,behavior,p,device);log_plugin("Requested video mode failed; restored native presentation");}
 if(SUCCEEDED(hr)){void **table=*(void***)*device;void *replacement;
  video_device=*device;video_window=focus;video_width=p->BackBufferWidth;video_height=p->BackBufferHeight;
  if(table[7])video_has_caps=SUCCEEDED(IDirect3DDevice9_GetDeviceCaps(video_device,&video_caps));
  video_reset=(void*)table[16];replacement=video_reset_device;patch(table+16,&replacement,sizeof(replacement));
  video_present=(void*)table[17];replacement=video_present_frame;patch(table+17,&replacement,sizeof(replacement));
  if(table[69]){video_sampler=(void*)table[69];replacement=video_set_sampler;patch(table+69,&replacement,sizeof(replacement));}
  video_window_style(focus);log_plugin(video_applied.mode==1?"Borderless presentation created":video_applied.mode==2?"Windowed presentation created":"Native fullscreen presentation created");
 }
 return hr;
}
static void video_install(void){
 video_load();video_create=(void*)call_target(0x880117);
 video_display_change=(void*)call_target(0x4055B8);
 if(video_display_change)replace_call(0x4055B8,video_change_display);
 else if(video_applied.mode){video_applied.mode=0;log_plugin("Display-change callback unavailable; native fullscreen preserved");}
 if(video_create)video_available=replace_call(0x880117,video_create_device);
 if(!video_available)log_plugin("Video presentation callback unavailable; native display preserved");
}
static DWORD video_detail_bias(void){float value=(float)performance.texture;DWORD bits;memcpy(&bits,&value,4);return bits;}
static void video_detail_begin(void){
 unsigned i;if(!video_device||!performance.texture)return;
 video_mip_active=1;
 for(i=0;i<8;i++){IDirect3DDevice9_GetSamplerState(video_device,i,D3DSAMP_MIPMAPLODBIAS,&video_mip_saved[i]);if(performance.texture)IDirect3DDevice9_SetSamplerState(video_device,i,D3DSAMP_MIPMAPLODBIAS,video_detail_bias());}
}
static void video_detail_end(void){
 unsigned i;if(!video_device||!video_mip_active)return;
 video_mip_active=0;for(i=0;i<8;i++)IDirect3DDevice9_SetSamplerState(video_device,i,D3DSAMP_MIPMAPLODBIAS,video_mip_saved[i]);
}
static void video_filter(void){
 static const DWORD anisotropy[]={0,1,1,2,4,8,16};D3DCAPS9 caps;DWORD level;unsigned i;
 if(!video_device||!video.filter)return;
 if(!video_has_caps)return;caps=video_caps;
 level=anisotropy[video.filter];if(level>caps.MaxAnisotropy)level=caps.MaxAnisotropy;
 for(i=0;i<8;i++){
  DWORD filter=video.filter==1?D3DTEXF_POINT:((level>1&&(caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC))?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR);
  IDirect3DDevice9_SetSamplerState(video_device,i,D3DSAMP_MINFILTER,filter);
  IDirect3DDevice9_SetSamplerState(video_device,i,D3DSAMP_MAGFILTER,video.filter==1?D3DTEXF_POINT:D3DTEXF_LINEAR);
  IDirect3DDevice9_SetSamplerState(video_device,i,D3DSAMP_MAXANISOTROPY,level?level:1);
 }
}
static HRESULT __stdcall video_set_sampler(IDirect3DDevice9 *device,DWORD sampler,D3DSAMPLERSTATETYPE state,DWORD value){
 static const DWORD anisotropy[]={0,1,1,2,4,8,16};
 if(device==video_device&&video_world_active&&performance.texture&&state==D3DSAMP_MIPMAPLODBIAS)value=video_detail_bias();
 if(device==video_device&&video_world_active&&video.filter){
  D3DCAPS9 caps;if(video_has_caps){DWORD level=anisotropy[video.filter];caps=video_caps;if(level>caps.MaxAnisotropy)level=caps.MaxAnisotropy;
   if(state==D3DSAMP_MINFILTER){value=video.filter==1?D3DTEXF_POINT:((level>1&&(caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC))?D3DTEXF_ANISOTROPIC:D3DTEXF_LINEAR);video_sampler(device,sampler,D3DSAMP_MAXANISOTROPY,level?level:1);}
   else if(state==D3DSAMP_MAGFILTER)value=video.filter==1?D3DTEXF_POINT:D3DTEXF_LINEAR;
   else if(state==D3DSAMP_MAXANISOTROPY)value=level?level:1;
  }
 }
 return video_sampler(device,sampler,state,value);
}
int FS_VideoOptions(lua_State *lua){
 unsigned i;if(lua_gettop(lua)){
  VideoSettings next;DWORD *values=(DWORD*)&next;
  for(i=0;i<6;i++){float f=luaL_checknumber(lua,i+1);if(!isfinite(f)||f<0||f>130||floorf(f)!=f)return luaL_error(lua,"Invalid video setting");values[i]=(DWORD)f;}
  if(!video_valid(&next))return luaL_error(lua,"Invalid video settings");
  if(!video_save(&next)){lua_pushboolean(lua,0);return 1;}video=next;lua_pushboolean(lua,1);return 1;
 }
 for(i=0;i<6;i++)lua_pushnumber(lua,((DWORD*)&video)[i]);return 6;
}
int FS_VideoStats(lua_State *lua){
 lua_pushboolean(lua,video_available);lua_pushnumber(lua,video_width);lua_pushnumber(lua,video_height);lua_pushnumber(lua,video_fps);
 lua_pushboolean(lua,video.mode!=video_applied.mode||video.vsync!=video_applied.vsync);return 5;
}
