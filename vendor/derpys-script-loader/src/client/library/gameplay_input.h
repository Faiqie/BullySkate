/* Bully action 5 is Pause; action 15 is Crouch. Keep bindings remappable. */
#define BS_PAUSE_ACTION 5
static void bs_mute_gameplay(game_controller *controller,unsigned pause_mask,int binding){
 unsigned char escape=(unsigned char)controller->input.keyboard[DIK_ESCAPE];
 unsigned char bound=(binding>=0&&binding<256)?controller->input.keyboard[binding]:0;
 short pause=controller->input.joystick.buttons&(short)pause_mask;
 int pressed=controller->pressed&(int)pause_mask,released=controller->released&(int)pause_mask;
 memset(&controller->input,0,sizeof(controller->input));
 controller->pressed=pressed;controller->released=released;
 controller->input.keyboard[DIK_ESCAPE]=(char)escape;controller->input.joystick.buttons=pause;
 if(binding>=0&&binding<256)controller->input.keyboard[binding]=(char)bound;
}
static void bs_context_action(game_controller *controller,unsigned mask,int binding){
 if(controller->is_joy){controller->input.joystick.buttons|=(short)mask;controller->pressed|=mask;}
 else if(binding>=0&&binding<256)controller->input.keyboard[binding]=0x80;
}
