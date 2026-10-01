/* Native Bully HAnim wrapper: 52-byte matrices and 20-byte named bone records.
 * First nine floats are rows of a column-vector rotation, then XYZ position.
 * Verified read-only against the running executable and its original NIF rig.
 * Override only during world drawing; restore every native matrix afterwards. */
typedef struct NativeRigBone {int parent,index,flags;void *frame;const char *name;} NativeRigBone;
typedef struct NativeRig {unsigned flags;int count;float (*matrices)[13];NativeRigBone *bones;} NativeRig;
static const char *rig_names[36]={"Dummy","Root","Root Pelvis","Root L Thigh","Root L Calf","Root L Foot",
 "Root R Thigh","Root R Calf","Root R Foot","Root01","Root Spine","Root Spine1","Root Spine2","Root Neck","Root Head",
 "Root Ponytail1","Root EyeLids","Root Brow","Root Eyes","Root L Clavicle","Root L UpperArm","Root L Forearm","Root L Hand",
 "Root L Finger0","Root L Finger1","Root L Finger11","Left_Shoulder","Root R Clavicle","Root R UpperArm","Root R Forearm","Root R Hand",
 "Root R Finger0","Root R Finger1","Root R Finger11","Right_Shoulder","ARROW"};
static float rig_backup[36][13];static float (*rig_restore)[13]=NULL;static unsigned rig_applied=0;
static float board_backup[2][13];static float (*board_restore)[13]=NULL;
/* Names/order belong to the immutable skeleton binding, not the current pose.
   Revalidate on binding changes; keep bounds checks on writable matrices. */
static NativeRig *rig_checked,*board_checked;
static NativeRigBone *rig_checked_bones,*board_checked_bones;
static char *rig_checked_owner,*board_checked_owner;
static int rig_checked_model;
/* Keep native blinking/brows/hair relative to the newly posed head. */
static int rig_head_delta(const float *old_head,const float *new_head,float delta[9]){
 float a=old_head[0],b=old_head[1],c=old_head[2],d=old_head[3],e=old_head[4],f=old_head[5],g=old_head[6],h=old_head[7],j=old_head[8];
 float det=a*(e*j-f*h)-b*(d*j-f*g)+c*(d*h-e*g),inv[9];unsigned r,k,n;
 if(!isfinite(det)||fabsf(det)<0.00001f)return 0;
 inv[0]=(e*j-f*h)/det;inv[1]=(c*h-b*j)/det;inv[2]=(b*f-c*e)/det;
 inv[3]=(f*g-d*j)/det;inv[4]=(a*j-c*g)/det;inv[5]=(c*d-a*f)/det;
 inv[6]=(d*h-e*g)/det;inv[7]=(b*g-a*h)/det;inv[8]=(a*e-b*d)/det;
 for(r=0;r<3;r++)for(k=0;k<3;k++){float v=0;for(n=0;n<3;n++)v+=new_head[r*3+n]*inv[n*3+k];delta[r*3+k]=v;}
 return 1;
}
static void rig_follow_head(const float *old_head,const float *new_head,const float *old_bone,const float delta[9],float out[13]){
 unsigned r,k,n;float offset[3]={old_bone[9]-old_head[9],old_bone[10]-old_head[10],old_bone[11]-old_head[11]};
 for(r=0;r<3;r++){
  for(k=0;k<3;k++){float v=0;for(n=0;n<3;n++)v+=delta[r*3+n]*old_bone[n*3+k];out[r*3+k]=v;}
  out[9+r]=new_head[9+r];for(n=0;n<3;n++)out[9+r]+=delta[r*3+n]*offset[n];
 }
 out[12]=old_bone[12];
}
static int FS_RigStatus(lua_State *lua){lua_pushboolean(lua,rig_active);lua_pushnumber(lua,rig_applied);return 2;}
void fakieBeforeWorld(void *arg){
 char *ped,*object,*frame,*child;NativeRig *rig;unsigned i;float head_delta[9];
 rig_restore=NULL;board_restore=NULL;
 if(!arg||!enabled||!rig_active)return;
 __try{
  ped=*(char**)0xC1AEA8;if(!readable(ped,0x1D4))return;
  object=*(char**)(ped+0x18);if(!readable(object,8))return;
  frame=*(char**)(object+4);if(!readable(frame,0x88))return;
  child=*(char**)(frame+0x84);if(!readable(child,0x98))return;
  rig=*(NativeRig**)(child+0x94);
  if(!readable(rig,sizeof(*rig))||rig->count!=36||!readable(rig->matrices,sizeof(rig_backup))||!readable(rig->bones,sizeof(NativeRigBone)*36))return;
  if(rig!=rig_checked||rig->bones!=rig_checked_bones||ped!=rig_checked_owner||*(short*)(ped+0x10E)!=rig_checked_model){
   for(i=0;i<36;i++)if(rig->bones[i].index!=(int)i||!readable(rig->bones[i].name,strlen(rig_names[i])+1)||strcmp(rig->bones[i].name,rig_names[i]))return;
   rig_checked=rig;rig_checked_bones=rig->bones;rig_checked_owner=ped;rig_checked_model=*(short*)(ped+0x10E);
  }
  memcpy(rig_backup,rig->matrices,sizeof(rig_backup));rig_restore=rig->matrices;
  memcpy(rig->matrices,rig_pose,sizeof(rig_pose));
  if(rig_head_delta(rig_backup[14],rig_pose[14],head_delta))for(i=15;i<=18;i++){
   if(rig->bones[i].parent==14)rig_follow_head(rig_backup[14],rig_pose[14],rig_backup[i],head_delta,rig->matrices[i]);
  }
  rig_applied++;
  object=*(char**)(ped+0x1D0);
  if(!readable(object,0x110)||*(short*)(object+0x10E)!=437)return;
  object=*(char**)(object+0x18);if(!readable(object,8))return;
  frame=*(char**)(object+4);if(!readable(frame,0x88))return;
  child=*(char**)(frame+0x84);if(!readable(child,0x98))return;
  rig=*(NativeRig**)(child+0x94);
  if(!readable(rig,sizeof(*rig))||rig->count!=2||!readable(rig->matrices,sizeof(board_backup))||!readable(rig->bones,2*sizeof(NativeRigBone)))return;
  if(rig!=board_checked||rig->bones!=board_checked_bones||child!=board_checked_owner){
   if(!readable(rig->bones[0].name,5)||strcmp(rig->bones[0].name,"Root")||!readable(rig->bones[1].name,11)||strcmp(rig->bones[1].name,"Bone_Board"))return;
   board_checked=rig;board_checked_bones=rig->bones;board_checked_owner=child;
  }
  memcpy(board_backup,rig->matrices,sizeof(board_backup));board_restore=rig->matrices;memcpy(rig->matrices,board_pose,sizeof(board_pose));
 }__except(EXCEPTION_EXECUTE_HANDLER){rig_active=0;}
}
void fakieAfterWorld(void){
 if(board_restore){__try{memcpy(board_restore,board_backup,sizeof(board_backup));}__except(EXCEPTION_EXECUTE_HANDLER){rig_active=0;}board_restore=NULL;}
 if(rig_restore){__try{memcpy(rig_restore,rig_backup,sizeof(rig_backup));}__except(EXCEPTION_EXECUTE_HANDLER){rig_active=0;}rig_restore=NULL;}
}
