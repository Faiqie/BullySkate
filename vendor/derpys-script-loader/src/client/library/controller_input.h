/* Direct DS4/DualSense input. SDL is loaded lazily on the game thread and only
 * initializes gamepads; Bully retains ownership of video, audio and its window.
 * The simulation keeps its existing XInput button/axis convention. */
#ifndef BULLYSKATE_CONTROLLER_INPUT_H
#define BULLYSKATE_CONTROLLER_INPUT_H
#include <windows.h>
#include <xinput.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_hints.h>

#ifndef BS_XINPUT_GET_STATE
#define BS_XINPUT_GET_STATE XInputGetState
#endif
typedef struct BS_PadState {
    int connected,kind; /* 1: XInput, 2: DS4, 3: DualSense */
    unsigned buttons;
    float lx,ly,rx,ry,lt,rt;
} BS_PadState;
typedef struct BS_ControllerInput {
    HMODULE library;
    SDL_Gamepad *pad;
    SDL_JoystickID id;
    ULONGLONG next_scan,next_xbox_scan;
    int tried,ready,kind,was_focused;
    unsigned source,blocked,xbox_slot;
    bool (SDLCALL *SetHint)(const char*,const char*);
    void (SDLCALL *SetMainReady)(void);
    bool (SDLCALL *InitSubSystem)(SDL_InitFlags);
    void (SDLCALL *QuitSubSystem)(SDL_InitFlags);
    void (SDLCALL *UpdateGamepads)(void);
    SDL_JoystickID *(SDLCALL *GetGamepads)(int*);
    void (SDLCALL *Free)(void*);
    SDL_GamepadType (SDLCALL *GetGamepadTypeForID)(SDL_JoystickID);
    SDL_Gamepad *(SDLCALL *OpenGamepad)(SDL_JoystickID);
    void (SDLCALL *CloseGamepad)(SDL_Gamepad*);
    bool (SDLCALL *GamepadConnected)(SDL_Gamepad*);
    bool (SDLCALL *GetGamepadButton)(SDL_Gamepad*,SDL_GamepadButton);
    Sint16 (SDLCALL *GetGamepadAxis)(SDL_Gamepad*,SDL_GamepadAxis);
    void (SDLCALL *SetGamepadEventsEnabled)(bool);
    void (SDLCALL *SetJoystickEventsEnabled)(bool);
} BS_ControllerInput;

static int bs_controller_init(BS_ControllerInput *input,const wchar_t *path) {
    if(input->tried)return input->ready;
    input->tried=1;
    /* Never search the current directory or PATH for this private dependency. */
    input->library=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!input->library)return 0;
#define BS_LOAD(member,name) do { *(FARPROC*)&input->member=GetProcAddress(input->library,name); if(!input->member)goto failed; } while(0)
    BS_LOAD(SetHint,"SDL_SetHint");BS_LOAD(SetMainReady,"SDL_SetMainReady");
    BS_LOAD(InitSubSystem,"SDL_InitSubSystem");BS_LOAD(QuitSubSystem,"SDL_QuitSubSystem");
    BS_LOAD(UpdateGamepads,"SDL_UpdateGamepads");BS_LOAD(GetGamepads,"SDL_GetGamepads");
    BS_LOAD(Free,"SDL_free");BS_LOAD(GetGamepadTypeForID,"SDL_GetGamepadTypeForID");
    BS_LOAD(OpenGamepad,"SDL_OpenGamepad");BS_LOAD(CloseGamepad,"SDL_CloseGamepad");
    BS_LOAD(GamepadConnected,"SDL_GamepadConnected");BS_LOAD(GetGamepadButton,"SDL_GetGamepadButton");
    BS_LOAD(GetGamepadAxis,"SDL_GetGamepadAxis");BS_LOAD(SetGamepadEventsEnabled,"SDL_SetGamepadEventsEnabled");
    BS_LOAD(SetJoystickEventsEnabled,"SDL_SetJoystickEventsEnabled");
#undef BS_LOAD
    input->SetMainReady();
    /* SDL has no window here. Our foreground check gates all returned input. */
    input->SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    input->SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4,"1");
    input->SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5,"1");
    input->SetHint(SDL_HINT_XINPUT_ENABLED,"0");
    input->SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");
    input->SetHint(SDL_HINT_JOYSTICK_WGI,"0");
    if(!input->InitSubSystem(SDL_INIT_GAMEPAD))goto failed;
    input->SetGamepadEventsEnabled(false);input->SetJoystickEventsEnabled(false);
    input->ready=1;return 1;
failed:
    FreeLibrary(input->library);input->library=NULL;return 0;
}
static void bs_controller_close(BS_ControllerInput *input) {
    if(input->pad)input->CloseGamepad(input->pad);
    if(input->ready)input->QuitSubSystem(SDL_INIT_GAMEPAD);
    if(input->library)FreeLibrary(input->library);
    memset(input,0,sizeof(*input));
}
static BS_PadState bs_controller_poll(BS_ControllerInput *input,const wchar_t *path,int focused,ULONGLONG now) {
    static const unsigned masks[]={4096,8192,16384,32768,32,0,16,64,128,256,512,1,2,4,8};
    BS_PadState state={0};unsigned source=0,i;XINPUT_STATE xbox={0};
    if(bs_controller_init(input,path)) {
        input->UpdateGamepads();
        if(input->pad&&!input->GamepadConnected(input->pad)) {
            input->CloseGamepad(input->pad);input->pad=NULL;input->id=0;input->next_scan=0;
        }
        /* No allocation/device enumeration in the steady-state skating path. */
        if(!input->pad&&now>=input->next_scan) {
            int count=0,k;SDL_JoystickID *ids=input->GetGamepads(&count);
            input->next_scan=now+1000;
            for(k=0;ids&&k<count;k++) {
                SDL_GamepadType type=input->GetGamepadTypeForID(ids[k]);
                if(type!=SDL_GAMEPAD_TYPE_PS4&&type!=SDL_GAMEPAD_TYPE_PS5)continue;
                input->pad=input->OpenGamepad(ids[k]);
                if(input->pad){input->id=ids[k];input->kind=type==SDL_GAMEPAD_TYPE_PS4?2:3;break;}
            }
            if(ids)input->Free(ids);
        }
        if(input->pad) {
            state.connected=1;state.kind=input->kind;source=0x80000000u|input->id;
            for(i=0;i<sizeof(masks)/sizeof(masks[0]);i++)
                if(masks[i]&&input->GetGamepadButton(input->pad,(SDL_GamepadButton)i))state.buttons|=masks[i];
            if(input->GetGamepadButton(input->pad,SDL_GAMEPAD_BUTTON_TOUCHPAD))state.buttons|=32;
            state.lx=input->GetGamepadAxis(input->pad,SDL_GAMEPAD_AXIS_LEFTX)/32768.0f;
            state.ly=-input->GetGamepadAxis(input->pad,SDL_GAMEPAD_AXIS_LEFTY)/32768.0f;
            state.rx=input->GetGamepadAxis(input->pad,SDL_GAMEPAD_AXIS_RIGHTX)/32768.0f;
            state.ry=-input->GetGamepadAxis(input->pad,SDL_GAMEPAD_AXIS_RIGHTY)/32768.0f;
            state.lt=input->GetGamepadAxis(input->pad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)/32767.0f;
            state.rt=input->GetGamepadAxis(input->pad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)/32767.0f;
            if(state.lt<0)state.lt=0;if(state.rt<0)state.rt=0;
        }
    }
    /* Prefer the physical PS pad over a duplicate Steam Input/DS4Windows pad.
     * When a mapper hides it, preserve the existing XInput fallback. */
    if(!state.connected) {
        int found=0;
        if(input->xbox_slot) {
            found=BS_XINPUT_GET_STATE(input->xbox_slot-1,&xbox)==ERROR_SUCCESS;
            if(!found){input->xbox_slot=0;input->next_xbox_scan=0;}
        }
        if(!found&&now>=input->next_xbox_scan) {
            input->next_xbox_scan=now+500;
            for(i=0;i<4;i++)if(BS_XINPUT_GET_STATE(i,&xbox)==ERROR_SUCCESS){input->xbox_slot=i+1;found=1;break;}
        }
        if(found) {
        state.connected=1;state.kind=1;source=input->xbox_slot;state.buttons=xbox.Gamepad.wButtons;
        state.lx=xbox.Gamepad.sThumbLX/32768.0f;state.ly=xbox.Gamepad.sThumbLY/32768.0f;
        state.rx=xbox.Gamepad.sThumbRX/32768.0f;state.ry=xbox.Gamepad.sThumbRY/32768.0f;
        state.lt=xbox.Gamepad.bLeftTrigger/255.0f;state.rt=xbox.Gamepad.bRightTrigger/255.0f;
        }
    }
    if(!focused||!input->was_focused||source!=input->source)input->blocked=state.buttons;
    input->blocked&=state.buttons;state.buttons&=~input->blocked;
    input->was_focused=focused;input->source=source;
    if(!focused){memset(&state,0,sizeof(state));}
    return state;
}
#endif
