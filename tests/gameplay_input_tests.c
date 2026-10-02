#include <dsl/dsl.h>
#include <stdio.h>
#include "gameplay_input.h"
#include <math.h>
#include "skater_preferences.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 char temporary[MAX_PATH],folder[MAX_PATH];FILE *file;DWORD version=1;
 SkaterPreferences legacy={{0,0.2f,0.8f,3,2,36,2,1,0,88,0}};
 game_controller c;memset(&c,0,sizeof(c));
 c.input.keyboard[DIK_ESCAPE]=0x80;c.input.keyboard[DIK_W]=0x80;
 c.input.keyboard[DIK_P]=0x80;c.input.joystick.buttons=0x1ff;c.pressed=0x1ff;c.released=0x1ff;
 c.input.joystick.y=1;c.input.mouse.lX=99;
 bs_mute_gameplay(&c,0x80,DIK_P);
 CHECK(c.input.keyboard[DIK_ESCAPE]&&c.input.keyboard[DIK_P]&&!c.input.keyboard[DIK_W]);
 CHECK(c.input.joystick.buttons==0x80&&c.pressed==0x80&&c.released==0x80);
 CHECK(!c.input.joystick.y&&!c.input.mouse.lX);
 bs_mute_gameplay(&c,0,-1);CHECK(c.input.keyboard[DIK_ESCAPE]&&!c.input.keyboard[DIK_P]);
 bs_context_action(&c,4,DIK_E);CHECK(c.input.keyboard[DIK_E]);
 c.is_joy=1;bs_context_action(&c,4,-1);CHECK(c.input.joystick.buttons==4&&c.pressed==4);
 CHECK(GetTempPathA(MAX_PATH,folder)&&GetTempFileNameA(folder,"bsk",0,temporary));
 skater_preferences_path=temporary;
 CHECK(fopen_s(&file,temporary,"wb")==0);
 CHECK(fwrite(&version,sizeof(version),1,file)==1&&fwrite(legacy.values,sizeof(float),10,file)==10);fclose(file);
 skater_preferences_load();CHECK(memcmp(skater_preferences.values,legacy.values,10*sizeof(float))==0&&skater_preferences.values[10]==0);
 version=2;legacy.values[10]=1;CHECK(fopen_s(&file,temporary,"wb")==0);
 CHECK(fwrite(&version,sizeof(version),1,file)==1&&fwrite(&legacy,sizeof(legacy),1,file)==1);fclose(file);
 skater_preferences_load();CHECK(memcmp(&skater_preferences,&legacy,sizeof(legacy))==0);
 legacy.values[10]=2;CHECK(fopen_s(&file,temporary,"wb")==0);
 CHECK(fwrite(&version,sizeof(version),1,file)==1&&fwrite(&legacy,sizeof(legacy),1,file)==1);fclose(file);
 skater_preferences_load();CHECK(memcmp(&skater_preferences,&skater_defaults,sizeof(skater_defaults))==0);
 CHECK(DeleteFileA(temporary));
 puts("PASS remapped pause, Escape, controller pause edges, gameplay suppression and context action");return 0;
}
