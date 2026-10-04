/* Ten finite, bounded values; a separate file preserves earlier preferences.
 * The same layout crosses the x86/x64 boundary without native pointers. */
typedef struct SkaterPreferences {float values[10];} SkaterPreferences;
static const SkaterPreferences skater_defaults={{1,0.7f,0.7f,0,0,0,1,2,3,67}};
static SkaterPreferences skater_preferences={{1,0.7f,0.7f,0,0,0,1,2,3,67}};
typedef struct SkateModeSettings {DWORD difficulty,camera_type;} SkateModeSettings;
static SkateModeSettings skate_mode_settings={0,1};
static int skate_mode_valid(const SkateModeSettings *p){return p->difficulty<=4&&p->camera_type<=1;}
static int skater_preferences_valid(const SkaterPreferences *p){
 unsigned i;const float *v=p->values;
 for(i=0;i<10;i++)if(!isfinite(v[i]))return 0;
 if(v[0]<0||v[0]>1||floorf(v[0])!=v[0])return 0;
 if(v[1]<0||v[1]>1||v[2]<0||v[2]>1)return 0;
 for(i=3;i<9;i++)if(v[i]<0||v[i]>(i<5?3:36)||floorf(v[i])!=v[i])return 0;
 return v[9]>=40&&v[9]<=110;
}
static const char *skater_preferences_path="_derpy_script_loader/scripts/BullyMotion/skater-settings.dat";
static void skater_preferences_load(void){
 FILE *f=NULL;SkaterPreferences loaded;DWORD version=0;
 skater_preferences=skater_defaults;skate_mode_settings=(SkateModeSettings){0,1};
 if(fopen_s(&f,skater_preferences_path,"rb")==0){
  SkateModeSettings modes={0,1};
  int valid=fread(&version,sizeof(version),1,f)==1&&(version>=1&&version<=4)&&fread(&loaded,sizeof(loaded),1,f)==1;
  /* Read the compatible ten-value prefix of the superseded local V2 build.
   * Preserve the original file until the user changes an actual preference. */
  if(valid&&version==2){float retired;valid=fread(&retired,sizeof(retired),1,f)==1;}
  if(valid&&version==3)valid=fread(&modes.difficulty,sizeof(DWORD),1,f)==1&&skate_mode_valid(&modes);
  if(valid&&version==4)valid=fread(&modes,sizeof(modes),1,f)==1&&skate_mode_valid(&modes);
  valid=valid&&fgetc(f)==EOF&&skater_preferences_valid(&loaded);
  fclose(f);if(valid){skater_preferences=loaded;skate_mode_settings=modes;}
 }
}
static int skater_preferences_save(const SkaterPreferences *p){
 const char *temporary="_derpy_script_loader/scripts/BullyMotion/skater-settings.dat.new";
 FILE *f=NULL;DWORD version=4;int valid;
 if(!skater_preferences_valid(p)||!skate_mode_valid(&skate_mode_settings)||fopen_s(&f,temporary,"wb"))return 0;
 valid=fwrite(&version,sizeof(version),1,f)==1&&fwrite(p,sizeof(*p),1,f)==1&&fwrite(&skate_mode_settings,sizeof(skate_mode_settings),1,f)==1;
 if(fflush(f))valid=0;
 if(fclose(f))valid=0;
 return valid&&MoveFileExA(temporary,skater_preferences_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
