/* Read nearby traffic on the Lua/game thread. The x64 worker receives values,
   never entity pointers. Pool generation bytes protect against reused slots. */
typedef struct VehicleHistory {DWORD id,time;float position[3];} VehicleHistory;
#include "vehicle_bounds.h"
static VehicleHistory vehicle_history[8];static unsigned vehicle_history_count;
static int FS_SkateVehicles(lua_State *lua){
 void **host=luaL_checkudata(lua,1,"BullyMotion.Skate");float x=luaL_checknumber(lua,2),y=luaL_checknumber(lua,3),z=luaL_checknumber(lua,4);
 float rows[8][16],distances[8],range=bs_traffic_range();VehicleHistory next[8];unsigned count=0,index,i,j,budget=bs_traffic_budget();DWORD now=GetTickCount();
 if(!enabled||!*host){lua_pushboolean(lua,0);return 1;}
 __try{
  game_pool *pool=getGameVehiclePool();
  if(readable(pool,sizeof(*pool))&&pool->limit<=512&&pool->size>=0x110&&pool->size<0x10000&&readable(pool->flags,pool->limit)){
   for(index=0;index<pool->limit;index++){
    char *entity,*matrix;const VehicleBounds *bounds;float row[16]={0},dx,dy,distance,length;int model;DWORD id;
    if(pool->flags[index]&GAME_POOL_INVALID)continue;
    entity=pool->array+index*pool->size;if(!readable(entity,0x110))continue;
    model=*(short*)(entity+0x10e);bounds=vehicle_model_bounds(model);if(!bounds)continue;
    matrix=*(char**)(entity+0x14);if(!readable(matrix,0x40))continue;
    memcpy(row+1,matrix+0x30,12);if(!isfinite(row[1])||!isfinite(row[2])||!isfinite(row[3]))continue;
    dx=row[1]-x;dy=row[2]-y;distance=dx*dx+dy*dy;
    if(distance>range*range||fabsf(row[3]-z)>6)continue;
    memcpy(row+4,matrix+0x10,12);length=sqrtf(row[4]*row[4]+row[5]*row[5]+row[6]*row[6]);
    if(!isfinite(length)||length<.5f||fabsf(row[6])>.45f)continue;
    row[4]/=length;row[5]/=length;row[6]/=length;
    for(i=0;i<3;i++){row[10+i]=(bounds->max[i]-bounds->min[i])*.5f;row[13+i]=(bounds->max[i]+bounds->min[i])*.5f;}
    if(!isfinite(row[10])||!isfinite(row[11])||!isfinite(row[12])||row[10]<.3f||row[10]>4||row[11]<.6f||row[11]>8||row[12]<.3f||row[12]>4)continue;
    if(!isfinite(row[13])||!isfinite(row[14])||!isfinite(row[15]))continue;
    id=(index<<8)|pool->flags[index];row[0]=(float)id;
    for(i=0;i<vehicle_history_count;i++)if(vehicle_history[i].id==id){
     float elapsed=(now-vehicle_history[i].time)*.001f;
     if(elapsed>=.01f&&elapsed<=.2f)for(j=0;j<3;j++)row[7+j]=(row[1+j]-vehicle_history[i].position[j])/elapsed;
     if(hypotf(row[7],row[8])>40||fabsf(row[9])>10)row[7]=row[8]=row[9]=0;
     break;
    }
    i=count;
    if(count==budget){i=budget-1;if(distance>=distances[i])continue;}else count++;
    while(i>0&&distance<distances[i-1]){memcpy(rows[i],rows[i-1],sizeof(row));distances[i]=distances[i-1];i--;}
    memcpy(rows[i],row,sizeof(row));distances[i]=distance;
   }
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){count=0;}
 for(i=0;i<count;i++){next[i].id=(DWORD)rows[i][0];next[i].time=now;memcpy(next[i].position,rows[i]+1,12);}
 memcpy(vehicle_history,next,count*sizeof(*next));vehicle_history_count=count;
 skate_vehicle_count=count;memcpy(skate_vehicles,rows,count*sizeof(*rows));skate_vehicle_revision++;
 lua_pushboolean(lua,1);return 1;
}
