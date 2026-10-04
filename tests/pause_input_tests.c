#include <dsl/dsl.h>
#include <assert.h>
#include "../native/gameplay_input.h"
int main(void){
 game_controller controller;unsigned masks[]={16,4096,512};unsigned k;
 assert(BS_PAUSE_ACTION==5);
 for(k=0;k<3;k++){
  unsigned mask=masks[k];memset(&controller,0,sizeof(controller));
  controller.input.joystick.buttons=(short)(mask|128|8192|32768);controller.pressed=mask|128;controller.released=mask|8192;
  controller.input.keyboard[DIK_ESCAPE]=0x80;controller.input.keyboard[37]=0x80;controller.input.keyboard[42]=0x80;
  controller.input.joystick.x=.8f;controller.input.joystick.y=.4f;controller.input.mouse.lX=300;
  bs_mute_gameplay(&controller,mask,37);
  assert(controller.input.joystick.buttons==mask&&controller.pressed==mask&&controller.released==mask);
  assert(controller.input.keyboard[DIK_ESCAPE]==(char)0x80&&controller.input.keyboard[37]==(char)0x80&&controller.input.keyboard[42]==0);
  assert(controller.input.joystick.x==0&&controller.input.joystick.y==0&&controller.input.mouse.lX==0);
 }
 puts("PASS: native Start/Options/Escape and a remapped pause binding survive skating suppression; movement and action inputs do not.");return 0;
}
