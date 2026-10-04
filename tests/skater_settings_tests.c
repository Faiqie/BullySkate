#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <direct.h>
#include "../native/skater_preferences.h"
static void old_file(DWORD version){FILE *f;float retired=1;assert(!fopen_s(&f,skater_preferences_path,"wb"));assert(fwrite(&version,4,1,f)==1);assert(fwrite(&skater_defaults,sizeof(skater_defaults),1,f)==1);if(version==2)assert(fwrite(&retired,4,1,f)==1);if(version==3){DWORD mode=2;assert(fwrite(&mode,4,1,f)==1);}fclose(f);}
int main(void){
 char previous[MAX_PATH],temporary[MAX_PATH],folder[MAX_PATH];DWORD version;FILE *f;
 assert(GetCurrentDirectoryA(sizeof(previous),previous));assert(GetTempPathA(sizeof(temporary),temporary));
 sprintf_s(folder,sizeof(folder),"%sBullySkatePrefs-%lu",temporary,GetCurrentProcessId());assert(!_mkdir(folder));assert(!_chdir(folder));
 assert(!_mkdir("_derpy_script_loader"));assert(!_mkdir("_derpy_script_loader/scripts"));assert(!_mkdir("_derpy_script_loader/scripts/BullyMotion"));
 for(version=1;version<=3;version++){old_file(version);skater_preferences_load();assert(skate_mode_settings.difficulty==(version==3?2:0)&&skate_mode_settings.camera_type==1);assert(skater_preferences.values[9]==67);assert(!fopen_s(&f,skater_preferences_path,"rb"));assert(fread(&version,4,1,f)==1);fclose(f);}
 for(unsigned camera=0;camera<=1;camera++)for(version=0;version<=4;version++){skate_mode_settings.camera_type=camera;skate_mode_settings.difficulty=version;skater_preferences.values[9]=83;assert(skater_preferences_save(&skater_preferences));skate_mode_settings.difficulty=0;skater_preferences_load();assert(skate_mode_settings.difficulty==version&&skate_mode_settings.camera_type==camera&&skater_preferences.values[9]==83);}
 skate_mode_settings.difficulty=5;assert(!skater_preferences_save(&skater_preferences));skater_preferences_load();assert(skate_mode_settings.difficulty==4&&skater_preferences.values[9]==83);
 skate_mode_settings.camera_type=2;assert(!skater_preferences_save(&skater_preferences));skater_preferences_load();assert(skate_mode_settings.camera_type==1);
 assert(DeleteFileA(skater_preferences_path));_rmdir("_derpy_script_loader/scripts/BullyMotion");_rmdir("_derpy_script_loader/scripts");_rmdir("_derpy_script_loader");assert(!_chdir(previous));assert(!_rmdir(folder));
 puts("PASS: V1/V2/V3 preferences preserved with High camera; both cameras and all five difficulties persist in V4; invalid settings keep the saved file");return 0;
}
