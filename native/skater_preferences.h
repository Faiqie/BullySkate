/* Eleven finite, bounded values; version 1 files retain their ten preferences.
 * The same layout crosses the x86/x64 boundary without native pointers. */
typedef struct SkaterPreferences {float values[11];} SkaterPreferences;
static const SkaterPreferences skater_defaults={{1,0.7f,0.7f,0,0,0,1,2,3,67,0}};
static SkaterPreferences skater_preferences={{1,0.7f,0.7f,0,0,0,1,2,3,67,0}};
static int skater_preferences_valid(const SkaterPreferences *p){
 unsigned i;const float *v=p->values;
 for(i=0;i<11;i++)if(!isfinite(v[i]))return 0;
 if(v[0]<0||v[0]>1||floorf(v[0])!=v[0])return 0;
 if(v[1]<0||v[1]>1||v[2]<0||v[2]>1)return 0;
 for(i=3;i<9;i++)if(v[i]<0||v[i]>(i<5?3:36)||floorf(v[i])!=v[i])return 0;
 return v[9]>=40&&v[9]<=110&&(v[10]==0||v[10]==1);
}
static const char *skater_preferences_path="_derpy_script_loader/scripts/BullyMotion/skater-settings.dat";
static void skater_preferences_load(void){
 FILE *f=NULL;SkaterPreferences loaded=skater_defaults;DWORD version=0;int valid=0;
 skater_preferences=skater_defaults;
 if(fopen_s(&f,skater_preferences_path,"rb")==0){
  if(fread(&version,sizeof(version),1,f)==1&&(version==1||version==2)){
   size_t count=version==1?10:11;
   valid=fread(loaded.values,sizeof(float),count,f)==count&&fgetc(f)==EOF&&skater_preferences_valid(&loaded);
  }
  fclose(f);if(valid)skater_preferences=loaded;
 }
}
static int skater_preferences_save(const SkaterPreferences *p){
 const char *temporary="_derpy_script_loader/scripts/BullyMotion/skater-settings.dat.new";
 FILE *f=NULL;DWORD version=2;int valid;
 if(!skater_preferences_valid(p)||fopen_s(&f,temporary,"wb"))return 0;
 valid=fwrite(&version,sizeof(version),1,f)==1&&fwrite(p,sizeof(*p),1,f)==1;
 if(fflush(f))valid=0;
 if(fclose(f))valid=0;
 return valid&&MoveFileExA(temporary,skater_preferences_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
