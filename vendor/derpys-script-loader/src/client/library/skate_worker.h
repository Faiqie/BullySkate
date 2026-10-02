/* Fixed-size shared memory between the native x86 adapter and x64 simulation.
 * No game pointers cross the boundary. Events own access to command/output. */
#include "skater_preferences.h"
typedef struct SkateInput {float dt;DWORD buttons;float axes[6];} SkateInput;
typedef struct SkateShared {
 volatile DWORD ready;DWORD command,success;
 float mount[4],dt;DWORD buttons;float axes[6];
 DWORD actor_count;float actors[24][9];float aspect;
 float output[14],pose[36][13],board[2][13],camera[7],root[3];
 DWORD has_camera;char error[2048];SkaterPreferences preferences;
 DWORD marker_flags,marker_sets,marker_returns;float marker_progress;
 DWORD input_count;SkateInput inputs[8];
 DWORD mount_area,actor_revision,vehicle_count,vehicle_revision;float vehicles[8][16];
 DWORD interaction[4];
} SkateShared;
_Static_assert(sizeof(SkateShared)==5920,"Skate worker protocol must match x64 layout");
typedef struct SkateSnapshot {
 float output[14],pose[36][13],board[2][13],camera[7],root[3];
 DWORD has_camera,marker_flags,marker_sets,marker_returns;float marker_progress;
 DWORD interaction[4];
} SkateSnapshot;
static SkateSnapshot skate_snapshot;
static SkateInput skate_inputs[8];static unsigned skate_input_count;
static float skate_actors[24][9];static DWORD skate_actor_count;
static DWORD skate_actor_revision,skate_vehicle_count,skate_vehicle_revision,skate_mount_area;
static float skate_vehicles[8][16];
static int skate_snapshot_valid;static DWORD skate_pending_type;
static unsigned skate_submitted,skate_reused,skate_overflows;
static float skate_aspect=16.0f/9.0f;
static SkateShared *skate_shared;
static HANDLE skate_mapping,skate_command,skate_response,skate_process,skate_job;
static int skate_pending=0,skate_started=0;
static int skate_channel_error(const char *message){
 if(skate_shared){strncpy_s(skate_shared->error,sizeof(skate_shared->error),message,_TRUNCATE);skate_shared->ready=3;}
 return 0;
}
static int skate_health(){
 if(!skate_started||!skate_shared)return 0;
 if(skate_process&&WaitForSingleObject(skate_process,0)==WAIT_OBJECT_0){
  char message[256];DWORD code=0;GetExitCodeProcess(skate_process,&code);
  sprintf_s(message,sizeof(message),"The Skate simulation worker exited (0x%08lX). See skate-worker.log; reopen Bully to restart it.",code);
  return skate_channel_error(message);
 }
 return skate_shared->ready==2;
}
static uint32_t ipc_skate_prepare(){
 if(!skate_started){
  WCHAR name[128],argument[256];STARTUPINFOW start;PROCESS_INFORMATION child;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
  SECURITY_ATTRIBUTES security={sizeof(SECURITY_ATTRIBUTES),NULL,TRUE};HANDLE log_file,input_file;BOOL launched;
  DWORD pid=GetCurrentProcessId();skate_started=1;
  swprintf_s(name,128,L"Local\\BullySkate-%lu-map",pid);
  skate_mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(SkateShared),name);
  if(!skate_mapping)return 3;
  skate_shared=(SkateShared*)MapViewOfFile(skate_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SkateShared));
  if(!skate_shared)return 3;
  memset(skate_shared,0,sizeof(*skate_shared));skate_shared->ready=1;skate_shared->aspect=16.0f/9.0f;
  skate_shared->preferences=skater_preferences;
  swprintf_s(name,128,L"Local\\BullySkate-%lu-command",pid);skate_command=CreateEventW(NULL,FALSE,FALSE,name);
  swprintf_s(name,128,L"Local\\BullySkate-%lu-response",pid);skate_response=CreateEventW(NULL,FALSE,FALSE,name);
  if(!skate_command||!skate_response){skate_channel_error("Cannot create the Skate simulation events.");return 3;}
  memset(&start,0,sizeof(start));start.cb=sizeof(start);memset(&child,0,sizeof(child));
  /* Source println diagnostics need valid independent handles after the
     terminal launcher closes. Never inherit its temporary output pipe. */
  log_file=CreateFileW(L"_derpy_script_loader\\logs\\skate-worker.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(log_file==INVALID_HANDLE_VALUE)log_file=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
  input_file=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
  start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=log_file;start.hStdError=log_file;start.hStdInput=input_file;
  swprintf_s(argument,256,L"\"SkatePhysicsWorker.exe\" --parent %lu",pid);
  launched=CreateProcessW(L"_derpy_script_loader\\scripts\\BullyMotion\\SkatePhysicsWorker.exe",argument,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&start,&child);
  if(log_file!=INVALID_HANDLE_VALUE)CloseHandle(log_file);if(input_file!=INVALID_HANDLE_VALUE)CloseHandle(input_file);
  if(!launched){
   skate_channel_error("SkatePhysicsWorker.exe could not start. Reopen the launcher to repair the installation.");return 3;
  }
  skate_process=child.hProcess;CloseHandle(child.hThread);
  skate_job=CreateJobObjectW(NULL,NULL);memset(&limits,0,sizeof(limits));limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(skate_job){SetInformationJobObject(skate_job,JobObjectExtendedLimitInformation,&limits,sizeof(limits));AssignProcessToJobObject(skate_job,skate_process);}
 }
 skate_health();return skate_shared?skate_shared->ready:3;
}
static uint32_t ipc_skate_error(char *out,uint32_t capacity){
 const char *message=skate_shared?skate_shared->error:"Cannot create the Skate simulation shared memory.";
 if(!out||!capacity)return 0;
 strncpy_s(out,capacity,message,_TRUNCATE);return (uint32_t)strlen(out);
}
static int skate_finish(DWORD timeout){
 if(!skate_pending)return 1;
 if(WaitForSingleObject(skate_response,timeout)!=WAIT_OBJECT_0)return 0;
 skate_pending=0;MemoryBarrier();
 if(skate_shared->success&&(skate_pending_type==1||skate_pending_type==2||skate_pending_type==4||skate_pending_type==5)){
  memcpy(skate_snapshot.output,skate_shared->output,sizeof(skate_snapshot.output));
  memcpy(skate_snapshot.pose,skate_shared->pose,sizeof(skate_snapshot.pose));
  memcpy(skate_snapshot.board,skate_shared->board,sizeof(skate_snapshot.board));
  memcpy(skate_snapshot.camera,skate_shared->camera,sizeof(skate_snapshot.camera));
  memcpy(skate_snapshot.root,skate_shared->root,sizeof(skate_snapshot.root));
  skate_snapshot.has_camera=skate_shared->has_camera;
  skate_snapshot.marker_flags=skate_shared->marker_flags;skate_snapshot.marker_sets=skate_shared->marker_sets;
  skate_snapshot.marker_returns=skate_shared->marker_returns;skate_snapshot.marker_progress=skate_shared->marker_progress;
  memcpy(skate_snapshot.interaction,skate_shared->interaction,sizeof(skate_snapshot.interaction));
  if(skate_pending_type==1)skate_snapshot_valid=1;
 }
 return 1;
}
static int skate_request(DWORD command,DWORD timeout){
 if(!skate_health()||!skate_finish(0))return 0;
 skate_shared->command=command;skate_shared->success=0;skate_pending_type=command;MemoryBarrier();skate_pending=1;
 SetEvent(skate_command);
 if(!skate_finish(timeout))return 0;
 return skate_shared->success!=0;
}
static void *ipc_skate_mount(float x,float y,float z,float yaw){
 if(!skate_health()||!skate_finish(40))return NULL;
 skate_shared->mount[0]=x;skate_shared->mount[1]=y;skate_shared->mount[2]=z;skate_shared->mount[3]=yaw;
 skate_shared->actor_count=0;skate_actor_count=0;skate_input_count=0;skate_snapshot_valid=0;
 skate_shared->input_count=0;
 skate_shared->mount_area=skate_mount_area;skate_vehicle_count=0;
 return skate_request(1,3000)?skate_shared:NULL;
}
static int ipc_skate_configure(const SkaterPreferences *p){
 if(!skater_preferences_valid(p))return 0;
 if(!skate_health()||!skate_finish(0))return 0;
 skate_shared->preferences=*p;
 return skate_request(4,40);
}
static void ipc_skate_release(void *handle){
 if(handle&&skate_health()&&skate_finish(25))skate_request(3,25);
 skate_input_count=0;skate_snapshot_valid=0;
}
static int ipc_skate_marker_clear(void){
 if(!skate_health()||!skate_finish(25))return 0;
 return skate_request(5,40);
}
static uint32_t ipc_skate_actors(void *handle,const float *records,uint32_t count){
 if(!handle||!records||count>24||!skate_health())return 0;
 skate_actor_count=count;memcpy(skate_actors,records,count*9*sizeof(float));skate_actor_revision++;return 1;
}
static uint32_t ipc_skate_step(void *handle,float dt,uint32_t buttons,float lx,float ly,float rx,float ry,float lt,float rt,float *out){
 float axes[6]={lx,ly,rx,ry,lt,rt};unsigned i;SkateInput *input;
 if(!handle||!out||!isfinite(dt)||!skate_health()||!skate_snapshot_valid)return 0;
 for(i=0;i<6;i++)if(!isfinite(axes[i]))return 0;
 skate_finish(0);
 if(!skate_health())return 0;
 /* Keep input samples while the worker is busy. Ordinary frames never wait
    for it; render the last complete, coherent rider/board/camera snapshot. */
 if(skate_input_count<8)input=&skate_inputs[skate_input_count++];
 else {input=&skate_inputs[7];dt+=input->dt;skate_overflows++;}
 input->dt=fmaxf(0,fminf(dt,0.1f));input->buttons=buttons;memcpy(input->axes,axes,sizeof(axes));
 if(!skate_pending){
  skate_shared->input_count=skate_input_count;memcpy(skate_shared->inputs,skate_inputs,skate_input_count*sizeof(SkateInput));
  skate_shared->dt=input->dt;skate_shared->buttons=input->buttons;memcpy(skate_shared->axes,input->axes,sizeof(input->axes));
  skate_shared->actor_count=skate_actor_count;memcpy(skate_shared->actors,skate_actors,skate_actor_count*9*sizeof(float));
  skate_shared->actor_revision=skate_actor_revision;
  skate_shared->vehicle_count=skate_vehicle_count;skate_shared->vehicle_revision=skate_vehicle_revision;
  memcpy(skate_shared->vehicles,skate_vehicles,skate_vehicle_count*16*sizeof(float));
  skate_shared->aspect=skate_aspect;
  skate_shared->command=2;skate_shared->success=0;skate_pending_type=2;
  MemoryBarrier();skate_pending=1;SetEvent(skate_command);skate_input_count=0;skate_submitted++;
 }else skate_reused++;
 memcpy(out,skate_snapshot.output,14*sizeof(float));return 1;
}
static uint32_t ipc_skate_pose(void *handle,float *out){
 if(!handle||!out||!skate_health()||!skate_snapshot_valid)return 0;memcpy(out,skate_snapshot.pose,sizeof(skate_snapshot.pose));return 1;
}
static uint32_t ipc_skate_board_pose(void *handle,float *out){
 if(!handle||!out||!skate_health()||!skate_snapshot_valid)return 0;memcpy(out,skate_snapshot.board,sizeof(skate_snapshot.board));return 1;
}
static uint32_t ipc_skate_camera(void *handle,float aspect,float *out){
 if(!handle||!out||!skate_health()||!skate_snapshot_valid||!isfinite(aspect)||aspect<=0)return 0;
 skate_aspect=aspect;
 if(!skate_snapshot.has_camera)return 0;memcpy(out,skate_snapshot.camera,sizeof(skate_snapshot.camera));return 1;
}
static uint32_t ipc_skate_root(void *handle,float *out){
 if(!handle||!out||!skate_health()||!skate_snapshot_valid)return 0;memcpy(out,skate_snapshot.root,sizeof(skate_snapshot.root));return 1;
}
#define fs_skate_prepare ipc_skate_prepare
#define fs_skate_error ipc_skate_error
#define fs_skate_mount ipc_skate_mount
#define fs_skate_release ipc_skate_release
#define fs_skate_step ipc_skate_step
#define fs_skate_pose ipc_skate_pose
#define fs_skate_board_pose ipc_skate_board_pose
#define fs_skate_camera ipc_skate_camera
#define fs_skate_root ipc_skate_root
#define fs_skate_actors ipc_skate_actors
