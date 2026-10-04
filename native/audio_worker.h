/* Nonblocking, pointer-free observation. The sound worker owns all mixing. */
typedef struct AudioShared {volatile LONG ready,sequence,active;DWORD version;float frame[64],camera[6];} AudioShared;
_Static_assert(sizeof(AudioShared)==296,"Audio wire layout must match Rust");
static AudioShared *audio_shared;
static HANDLE audio_mapping,audio_process;
static int audio_started;
static unsigned audio_prepare(void){
 if(!audio_started){
  WCHAR name[128],argument[128];STARTUPINFOW start={sizeof(start)};PROCESS_INFORMATION child={0};
  SECURITY_ATTRIBUTES security={sizeof(security),NULL,TRUE};HANDLE out,in;BOOL launched;
  audio_started=1;swprintf_s(name,128,L"Local\\BullySkateAudio-%lu",GetCurrentProcessId());
  audio_mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(AudioShared),name);
  if(!audio_mapping)return 3;
  audio_shared=MapViewOfFile(audio_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(AudioShared));
  if(!audio_shared)return 3;
  memset(audio_shared,0,sizeof(*audio_shared));audio_shared->version=1;audio_shared->ready=1;
  out=CreateFileW(L"_derpy_script_loader\\logs\\skate-audio.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_ALWAYS,0,NULL);
  if(out==INVALID_HANDLE_VALUE)out=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
  in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
  start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=out;start.hStdError=out;start.hStdInput=in;
  swprintf_s(argument,128,L"\"SkateAudioWorker.exe\" --parent %lu",GetCurrentProcessId());
  launched=CreateProcessW(L"_derpy_script_loader\\scripts\\BullyMotion\\SkateAudioWorker.exe",argument,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&start,&child);
  if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);if(in!=INVALID_HANDLE_VALUE)CloseHandle(in);
  if(!launched){audio_shared->ready=3;return 3;}
  audio_process=child.hProcess;CloseHandle(child.hThread);
  if(skate_job)AssignProcessToJobObject(skate_job,audio_process);
 }
 if(audio_process&&WaitForSingleObject(audio_process,0)==WAIT_OBJECT_0)audio_shared->ready=3;
 return audio_shared?(unsigned)audio_shared->ready:3;
}
static void audio_write(int active){
 if(!audio_shared)return;
 InterlockedIncrement(&audio_shared->sequence);MemoryBarrier();
 audio_shared->active=active&&skate_snapshot_valid;
 if(audio_shared->active){
  const float *c=skate_snapshot.camera;memcpy(audio_shared->frame,skate_snapshot.audio,sizeof(audio_shared->frame));
  /* Camera is published in Bully's Z-up space; audio/map use Skate's Y-up. */
  audio_shared->camera[0]=c[0];audio_shared->camera[1]=c[2];audio_shared->camera[2]=-c[1];
  audio_shared->camera[3]=c[3];audio_shared->camera[4]=c[5];audio_shared->camera[5]=-c[4];
 }
 MemoryBarrier();InterlockedIncrement(&audio_shared->sequence);
}
static int FS_AudioReady(lua_State *lua){lua_pushnumber(lua,audio_prepare());return 1;}
static int FS_AudioStep(lua_State *lua){unsigned ready=audio_prepare();audio_write(ready==2);lua_pushboolean(lua,ready==2);return 1;}
static int FS_AudioStop(lua_State *lua){audio_write(0);return 0;}
