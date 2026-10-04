/* Live settings have their own versioned file, preserving video/skater saves.
 * Collision budgets affect the skating bridge, never native NPC/traffic AI. */
typedef struct PerformanceSettings {DWORD texture,npcs,npc_range,npc_rate,cars,car_range,car_rate,audio,hints;} PerformanceSettings;
static const PerformanceSettings performance_defaults={0,2,2,1,1,2,1,1,1};
static PerformanceSettings performance={0,2,2,1,1,2,1,1,1};
static int performance_valid(const PerformanceSettings *p){
 return p->texture<=2&&p->npcs<=2&&p->npc_range<=2&&p->npc_rate<=2&&p->cars<=1&&p->car_range<=2&&p->car_rate<=2&&p->audio<=1&&p->hints<=1;
}
static void performance_load(void){
 FILE *f;DWORD version;PerformanceSettings next;
 if(!fopen_s(&f,"_derpy_script_loader/scripts/BullyMotion/performance-settings.dat","rb")){
  int valid=fread(&version,4,1,f)==1&&version==1&&fread(&next,sizeof(next),1,f)==1&&fgetc(f)==EOF&&performance_valid(&next);
  fclose(f);if(valid)performance=next;
 }
}
int FS_PerformanceOptions(lua_State *lua){
 unsigned i;if(lua_gettop(lua)){
  PerformanceSettings next;DWORD *values=(DWORD*)&next;FILE *f;DWORD version=1;int valid;
  for(i=0;i<9;i++){float value=luaL_checknumber(lua,i+1);if(!isfinite(value)||value<0||value>2||floorf(value)!=value)return luaL_error(lua,"Invalid performance setting");values[i]=(DWORD)value;}
  if(!performance_valid(&next))return luaL_error(lua,"Invalid performance settings");
  if(fopen_s(&f,"_derpy_script_loader/scripts/BullyMotion/performance-settings.dat.new","wb")){lua_pushboolean(lua,0);return 1;}
  valid=fwrite(&version,4,1,f)==1&&fwrite(&next,sizeof(next),1,f)==1;
  if(fflush(f))valid=0;if(fclose(f))valid=0;
  valid=valid&&MoveFileExA("_derpy_script_loader/scripts/BullyMotion/performance-settings.dat.new","_derpy_script_loader/scripts/BullyMotion/performance-settings.dat",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
  if(valid)performance=next;lua_pushboolean(lua,valid);return 1;
 }
 for(i=0;i<9;i++)lua_pushnumber(lua,((DWORD*)&performance)[i]);return 9;
}
unsigned bs_traffic_budget(void){static const unsigned counts[]={4,8};return counts[performance.cars];}
float bs_traffic_range(void){static const float ranges[]={15,25,35};return ranges[performance.car_range];}
