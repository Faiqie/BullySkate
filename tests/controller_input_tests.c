#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include <math.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_hints.h>
static int xbox_connected;
static XINPUT_STATE xbox_state;
static DWORD WINAPI test_xinput(DWORD index,XINPUT_STATE *out) {
    if(index||!xbox_connected)return ERROR_DEVICE_NOT_CONNECTED;
    *out=xbox_state;return ERROR_SUCCESS;
}
#define BS_XINPUT_GET_STATE test_xinput
#include "controller_input.h"
#define CHECK(expr) do { if(!(expr)){fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#expr,SDL_GetError());exit(1);} } while(0)
static BS_ControllerInput input;
static wchar_t library[MAX_PATH];
static ULONGLONG tick=1000;
static BS_PadState poll(int focus) {return bs_controller_poll(&input,library,focus,tick+=1100);}
static SDL_JoystickID attach(const char *type,Uint16 product,SDL_Joystick **joy) {
    SDL_VirtualJoystickDesc desc;char guid[33],mapping[1024];SDL_JoystickID id;
    SDL_INIT_INTERFACE(&desc);desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=6;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.vendor_id=0x054c;desc.product_id=product;desc.name="BullySkate virtual controller test";
    id=SDL_AttachVirtualJoystick(&desc);CHECK(id!=0);
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(id),guid,sizeof(guid));
    snprintf(mapping,sizeof(mapping),"%s,Test %s,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,touchpad:b20,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,type:%s,",guid,type,type);
    CHECK(SDL_AddGamepadMapping(mapping)>=0);
    *joy=SDL_OpenJoystick(id);CHECK(*joy!=NULL);
    CHECK(SDL_SetJoystickVirtualAxis(*joy,4,-32768));CHECK(SDL_SetJoystickVirtualAxis(*joy,5,-32768));
    return id;
}
static void button(SDL_Joystick *joy,int index,int down) {CHECK(SDL_SetJoystickVirtualButton(joy,index,down!=0));}
static void run_ps(const char *type,Uint16 product,int expected_kind) {
    static const unsigned masks[]={4096,8192,16384,32768,32,0,16,64,128,256,512,1,2,4,8};
    SDL_Joystick *joy;SDL_JoystickID id=attach(type,product,&joy);BS_PadState state;int k;
    state=poll(1);CHECK(state.connected&&state.kind==expected_kind);CHECK(state.buttons==0);
    CHECK(state.lt==0&&state.rt==0);
    for(k=0;k<15;k++) {
        button(joy,k,1);state=poll(1);CHECK(state.buttons==masks[k]);
        button(joy,k,0);CHECK(poll(1).buttons==0);
    }
    button(joy,SDL_GAMEPAD_BUTTON_TOUCHPAD,1);CHECK(poll(1).buttons==32);
    button(joy,SDL_GAMEPAD_BUTTON_TOUCHPAD,0);poll(1);
    CHECK(SDL_SetJoystickVirtualAxis(joy,0,16384));CHECK(SDL_SetJoystickVirtualAxis(joy,1,-16384));
    CHECK(SDL_SetJoystickVirtualAxis(joy,2,-24576));CHECK(SDL_SetJoystickVirtualAxis(joy,3,24576));
    CHECK(SDL_SetJoystickVirtualAxis(joy,4,0));CHECK(SDL_SetJoystickVirtualAxis(joy,5,32767));
    state=poll(1);CHECK(fabsf(state.lx-.5f)<.001f&&fabsf(state.ly-.5f)<.001f);
    CHECK(fabsf(state.rx+.75f)<.001f&&fabsf(state.ry+.75f)<.001f);
    CHECK(fabsf(state.lt-.5f)<.001f&&state.rt==1);
    /* A physical pad takes precedence over its duplicate emulated XInput pad. */
    xbox_connected=1;xbox_state.Gamepad.wButtons=8192;
    button(joy,0,1);CHECK(poll(1).buttons==4096);
    state=poll(0);CHECK(!state.connected&&state.buttons==0&&state.lx==0&&state.rt==0);
    state=poll(1);CHECK(state.connected&&state.buttons==0);
    CHECK(poll(1).buttons==0); /* Held across focus changes stays suppressed. */
    button(joy,0,0);poll(1);button(joy,0,1);CHECK(poll(1).buttons==4096);
    button(joy,9,1);button(joy,12,1);CHECK((poll(1).buttons&(256|2))==(256|2));
    button(joy,4,1);button(joy,13,1);CHECK((poll(1).buttons&(32|4))==(32|4));
    {
        LARGE_INTEGER begin,end,frequency;int frame;
        ULONGLONG scan=input.next_scan;
        QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
        for(frame=0;frame<2000;frame++)CHECK(poll(1).kind==expected_kind);
        QueryPerformanceCounter(&end);CHECK(input.next_scan==scan);
        printf("%s input polling: %.4f ms average (virtual device, 2000 frames)\n",type,(end.QuadPart-begin.QuadPart)*1000.0/frequency.QuadPart/2000);
    }
    SDL_CloseJoystick(joy);CHECK(SDL_DetachVirtualJoystick(id));
    state=poll(1);CHECK(state.connected&&state.kind==1&&state.buttons==0);
    xbox_state.Gamepad.wButtons=0;poll(1);xbox_state.Gamepad.wButtons=8192;CHECK(poll(1).buttons==8192);
    xbox_connected=0;state=poll(1);CHECK(!state.connected&&state.buttons==0&&state.lx==0&&state.rt==0);
    printf("PASS %s buttons, sticks, triggers, shortcuts, focus, disconnect and XInput fallback\n",type);
}
int wmain(int argc,wchar_t **argv) {
    BS_PadState state;BS_ControllerInput missing={0};
    CHECK(argc==2);CHECK(wcslen(argv[1])<MAX_PATH);wcscpy_s(library,MAX_PATH,argv[1]);
    CHECK(bs_controller_init(&input,library));
    CHECK(!SDL_WasInit(SDL_INIT_VIDEO|SDL_INIT_AUDIO));
    CHECK(!SDL_GamepadEventsEnabled()&&!SDL_JoystickEventsEnabled());
    CHECK(!poll(1).connected);
    run_ps("ps4",0x05c4,2);run_ps("ps5",0x0ce6,3);
    xbox_connected=1;xbox_state.Gamepad.sThumbLX=-32768;xbox_state.Gamepad.sThumbLY=16384;
    xbox_state.Gamepad.bRightTrigger=255;xbox_state.Gamepad.wButtons=0;
    state=bs_controller_poll(&missing,L"Z:\\missing-BullySkate-SDL3.dll",1,tick++);
    CHECK(state.connected&&state.kind==1&&state.lx==-1&&state.ly==.5f&&state.rt==1);
    bs_controller_close(&input);bs_controller_close(&missing);
    puts("PASS optional SDL failure preserves Xbox; no video/audio initialization");return 0;
}
