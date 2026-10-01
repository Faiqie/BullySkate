// DERPY'S SCRIPT LOADER: INPUT LIBRARY

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>

#define REGISTRY_KEY DSL_REGISTRY_KEY "_input"

#define TYPING_BYTES 1024
#define STATUS_TYPING 0
#define STATUS_ABORTED 1
#define STATUS_FINISHED 2

// TYPING STRUCT
typedef struct typing_data{
	int status;
	size_t count;
	size_t alloc;
	char *buffer;
}typing_data;

// DIK NAMES
static const struct dik_key{
	const char *name;
	int code;
}dik_keys[] = {
	{"ESCAPE",DIK_ESCAPE},
	{"1",DIK_1},
	{"2",DIK_2},
	{"3",DIK_3},
	{"4",DIK_4},
	{"5",DIK_5},
	{"6",DIK_6},
	{"7",DIK_7},
	{"8",DIK_8},
	{"9",DIK_9},
	{"0",DIK_0},
	{"MINUS",DIK_MINUS},
	{"EQUALS",DIK_EQUALS},
	{"BACK",DIK_BACK},
	{"TAB",DIK_TAB},
	{"Q",DIK_Q},
	{"W",DIK_W},
	{"E",DIK_E},
	{"R",DIK_R},
	{"T",DIK_T},
	{"Y",DIK_Y},
	{"U",DIK_U},
	{"I",DIK_I},
	{"O",DIK_O},
	{"P",DIK_P},
	{"LBRACKET",DIK_LBRACKET},
	{"RBRACKET",DIK_RBRACKET},
	{"RETURN",DIK_RETURN},
	{"LCONTROL",DIK_LCONTROL},
	{"A",DIK_A},
	{"S",DIK_S},
	{"D",DIK_D},
	{"F",DIK_F},
	{"G",DIK_G},
	{"H",DIK_H},
	{"J",DIK_J},
	{"K",DIK_K},
	{"L",DIK_L},
	{"SEMICOLON",DIK_SEMICOLON},
	{"APOSTROPHE",DIK_APOSTROPHE},
	{"GRAVE",DIK_GRAVE},
	{"LSHIFT",DIK_LSHIFT},
	{"BACKSLASH",DIK_BACKSLASH},
	{"Z",DIK_Z},
	{"X",DIK_X},
	{"C",DIK_C},
	{"V",DIK_V},
	{"B",DIK_B},
	{"N",DIK_N},
	{"M",DIK_M},
	{"COMMA",DIK_COMMA},
	{"PERIOD",DIK_PERIOD},
	{"SLASH",DIK_SLASH},
	{"RSHIFT",DIK_RSHIFT},
	{"MULTIPLY",DIK_MULTIPLY},
	{"LMENU",DIK_LMENU},
	{"SPACE",DIK_SPACE},
	{"CAPITAL",DIK_CAPITAL},
	{"F1",DIK_F1},
	{"F2",DIK_F2},
	{"F3",DIK_F3},
	{"F4",DIK_F4},
	{"F5",DIK_F5},
	{"F6",DIK_F6},
	{"F7",DIK_F7},
	{"F8",DIK_F8},
	{"F9",DIK_F9},
	{"F10",DIK_F10},
	{"NUMLOCK",DIK_NUMLOCK},
	{"SCROLL",DIK_SCROLL},
	{"NUMPAD7",DIK_NUMPAD7},
	{"NUMPAD8",DIK_NUMPAD8},
	{"NUMPAD9",DIK_NUMPAD9},
	{"SUBTRACT",DIK_SUBTRACT},
	{"NUMPAD4",DIK_NUMPAD4},
	{"NUMPAD5",DIK_NUMPAD5},
	{"NUMPAD6",DIK_NUMPAD6},
	{"ADD",DIK_ADD},
	{"NUMPAD1",DIK_NUMPAD1},
	{"NUMPAD2",DIK_NUMPAD2},
	{"NUMPAD3",DIK_NUMPAD3},
	{"NUMPAD0",DIK_NUMPAD0},
	{"DECIMAL",DIK_DECIMAL},
	{"OEM_102",DIK_OEM_102},
	{"F11",DIK_F11},
	{"F12",DIK_F12},
	{"F13",DIK_F13},
	{"F14",DIK_F14},
	{"F15",DIK_F15},
	{"KANA",DIK_KANA},
	{"ABNT_C1",DIK_ABNT_C1},
	{"CONVERT",DIK_CONVERT},
	{"NOCONVERT",DIK_NOCONVERT},
	{"YEN",DIK_YEN},
	{"ABNT_C2",DIK_ABNT_C2},
	{"NUMPADEQUALS",DIK_NUMPADEQUALS},
	{"PREVTRACK",DIK_PREVTRACK},
	{"AT",DIK_AT},
	{"COLON",DIK_COLON},
	{"UNDERLINE",DIK_UNDERLINE},
	{"KANJI",DIK_KANJI},
	{"STOP",DIK_STOP},
	{"AX",DIK_AX},
	{"UNLABELED",DIK_UNLABELED},
	{"NEXTTRACK",DIK_NEXTTRACK},
	{"NUMPADENTER",DIK_NUMPADENTER},
	{"RCONTROL",DIK_RCONTROL},
	{"MUTE",DIK_MUTE},
	{"CALCULATOR",DIK_CALCULATOR},
	{"PLAYPAUSE",DIK_PLAYPAUSE},
	{"MEDIASTOP",DIK_MEDIASTOP},
	{"VOLUMEDOWN",DIK_VOLUMEDOWN},
	{"VOLUMEUP",DIK_VOLUMEUP},
	{"WEBHOME",DIK_WEBHOME},
	{"NUMPADCOMMA",DIK_NUMPADCOMMA},
	{"DIVIDE",DIK_DIVIDE},
	{"SYSRQ",DIK_SYSRQ},
	{"RMENU",DIK_RMENU},
	{"PAUSE",DIK_PAUSE},
	{"HOME",DIK_HOME},
	{"UP",DIK_UP},
	{"PRIOR",DIK_PRIOR},
	{"LEFT",DIK_LEFT},
	{"RIGHT",DIK_RIGHT},
	{"END",DIK_END},
	{"DOWN",DIK_DOWN},
	{"NEXT",DIK_NEXT},
	{"INSERT",DIK_INSERT},
	{"DELETE",DIK_DELETE},
	{"LWIN",DIK_LWIN},
	{"RWIN",DIK_RWIN},
	{"APPS",DIK_APPS},
	{"POWER",DIK_POWER},
	{"SLEEP",DIK_SLEEP},
	{"WAKE",DIK_WAKE},
	{"WEBSEARCH",DIK_WEBSEARCH},
	{"WEBFAVORITES",DIK_WEBFAVORITES},
	{"WEBREFRESH",DIK_WEBREFRESH},
	{"WEBSTOP",DIK_WEBSTOP},
	{"WEBFORWARD",DIK_WEBFORWARD},
	{"WEBBACK",DIK_WEBBACK},
	{"MYCOMPUTER",DIK_MYCOMPUTER},
	{"MAIL",DIK_MAIL},
	{"MEDIASELECT",DIK_MEDIASELECT},
	{"BACKSPACE",DIK_BACK},
	{"NUMPADSTAR",DIK_MULTIPLY},
	{"LALT",DIK_LMENU},
	{"CAPSLOCK",DIK_CAPITAL},
	{"NUMPADMINUS",DIK_SUBTRACT},
	{"NUMPADPLUS",DIK_ADD},
	{"NUMPADPERIOD",DIK_DECIMAL},
	{"NUMPADSLASH",DIK_DIVIDE},
	{"RALT",DIK_RMENU},
	{"UPARROW",DIK_UP},
	{"PGUP",DIK_PRIOR},
	{"LEFTARROW",DIK_LEFT},
	{"RIGHTARROW",DIK_RIGHT},
	{"DOWNARROW",DIK_DOWN},
	{"PGDN",DIK_NEXT},
	{"CIRCUMFLEX",DIK_PREVTRACK}
};

// GET CONTROLLER
static game_controller* getController(lua_State *lua,int stack){
	int id;
	
	luaL_checktype(lua,stack,LUA_TNUMBER);
	id = lua_tonumber(lua,stack);
	if(id < 0 || id > 3)
		luaL_argerror(lua,stack,"invalid controller");
	if(id == 0)
		id = getGamePrimaryControllerIndex();
	else if(id == 1)
		id = getGameSecondaryControllerIndex();
	return getGameControllers() + id;
}

// GET KEYBOARD
static int getDikFromString(const char *dik){
	int n;
	
	n = sizeof(dik_keys) / sizeof(struct dik_key);
	while(n)
		if(dslisenum(dik,dik_keys[--n].name))
			return dik_keys[n].code;
	if(toupper(*dik) == 'D' && toupper(*(++dik)) == 'I' && toupper(*(++dik)) == 'K' && *(++dik) == '_')
		return getDikFromString(dik+1);
	return 0;
}
static int getVkFromString(const char *vk){
	if(*vk && !vk[1]){
		if(*vk >= 'A' && *vk <= 'Z')
			return 0x41 + (toupper(*vk) - 'A');
		if(*vk >= '0' && *vk <= '9')
			return 0x30 + (*vk - '0');
	}
	if(dslisenum(vk,"VK_LBUTTON"))
		return 0x01;
	if(dslisenum(vk,"VK_RBUTTON"))
		return 0x02;
	if(dslisenum(vk,"VK_CANCEL"))
		return 0x03;
	if(dslisenum(vk,"VK_MBUTTON"))
		return 0x04;
	if(dslisenum(vk,"VK_XBUTTON1"))
		return 0x05;
	if(dslisenum(vk,"VK_XBUTTON2"))
		return 0x06;
	if(dslisenum(vk,"VK_BACK"))
		return 0x08;
	if(dslisenum(vk,"VK_TAB"))
		return 0x09;
	if(dslisenum(vk,"VK_CLEAR"))
		return 0x0C;
	if(dslisenum(vk,"VK_RETURN"))
		return 0x0D;
	if(dslisenum(vk,"VK_SHIFT"))
		return 0x10;
	if(dslisenum(vk,"VK_CONTROL"))
		return 0x11;
	if(dslisenum(vk,"VK_MENU"))
		return 0x12;
	if(dslisenum(vk,"VK_PAUSE"))
		return 0x13;
	if(dslisenum(vk,"VK_CAPITAL"))
		return 0x14;
	if(dslisenum(vk,"VK_KANA"))
		return 0x15;
	if(dslisenum(vk,"VK_HANGEUL"))
		return 0x15;
	if(dslisenum(vk,"VK_HANGUL"))
		return 0x15;
	if(dslisenum(vk,"VK_IME_ON"))
		return 0x16;
	if(dslisenum(vk,"VK_JUNJA"))
		return 0x17;
	if(dslisenum(vk,"VK_FINAL"))
		return 0x18;
	if(dslisenum(vk,"VK_HANJA"))
		return 0x19;
	if(dslisenum(vk,"VK_KANJI"))
		return 0x19;
	if(dslisenum(vk,"VK_IME_OFF"))
		return 0x1A;
	if(dslisenum(vk,"VK_ESCAPE"))
		return 0x1B;
	if(dslisenum(vk,"VK_CONVERT"))
		return 0x1C;
	if(dslisenum(vk,"VK_NONCONVERT"))
		return 0x1D;
	if(dslisenum(vk,"VK_ACCEPT"))
		return 0x1E;
	if(dslisenum(vk,"VK_MODECHANGE"))
		return 0x1F;
	if(dslisenum(vk,"VK_SPACE"))
		return 0x20;
	if(dslisenum(vk,"VK_PRIOR"))
		return 0x21;
	if(dslisenum(vk,"VK_NEXT"))
		return 0x22;
	if(dslisenum(vk,"VK_END"))
		return 0x23;
	if(dslisenum(vk,"VK_HOME"))
		return 0x24;
	if(dslisenum(vk,"VK_LEFT"))
		return 0x25;
	if(dslisenum(vk,"VK_UP"))
		return 0x26;
	if(dslisenum(vk,"VK_RIGHT"))
		return 0x27;
	if(dslisenum(vk,"VK_DOWN"))
		return 0x28;
	if(dslisenum(vk,"VK_SELECT"))
		return 0x29;
	if(dslisenum(vk,"VK_PRINT"))
		return 0x2A;
	if(dslisenum(vk,"VK_EXECUTE"))
		return 0x2B;
	if(dslisenum(vk,"VK_SNAPSHOT"))
		return 0x2C;
	if(dslisenum(vk,"VK_INSERT"))
		return 0x2D;
	if(dslisenum(vk,"VK_DELETE"))
		return 0x2E;
	if(dslisenum(vk,"VK_HELP"))
		return 0x2F;
	if(dslisenum(vk,"VK_LWIN"))
		return 0x5B;
	if(dslisenum(vk,"VK_RWIN"))
		return 0x5C;
	if(dslisenum(vk,"VK_APPS"))
		return 0x5D;
	if(dslisenum(vk,"VK_SLEEP"))
		return 0x5F;
	if(dslisenum(vk,"VK_NUMPAD0"))
		return 0x60;
	if(dslisenum(vk,"VK_NUMPAD1"))
		return 0x61;
	if(dslisenum(vk,"VK_NUMPAD2"))
		return 0x62;
	if(dslisenum(vk,"VK_NUMPAD3"))
		return 0x63;
	if(dslisenum(vk,"VK_NUMPAD4"))
		return 0x64;
	if(dslisenum(vk,"VK_NUMPAD5"))
		return 0x65;
	if(dslisenum(vk,"VK_NUMPAD6"))
		return 0x66;
	if(dslisenum(vk,"VK_NUMPAD7"))
		return 0x67;
	if(dslisenum(vk,"VK_NUMPAD8"))
		return 0x68;
	if(dslisenum(vk,"VK_NUMPAD9"))
		return 0x69;
	if(dslisenum(vk,"VK_MULTIPLY"))
		return 0x6A;
	if(dslisenum(vk,"VK_ADD"))
		return 0x6B;
	if(dslisenum(vk,"VK_SEPARATOR"))
		return 0x6C;
	if(dslisenum(vk,"VK_SUBTRACT"))
		return 0x6D;
	if(dslisenum(vk,"VK_DECIMAL"))
		return 0x6E;
	if(dslisenum(vk,"VK_DIVIDE"))
		return 0x6F;
	if(dslisenum(vk,"VK_F1"))
		return 0x70;
	if(dslisenum(vk,"VK_F2"))
		return 0x71;
	if(dslisenum(vk,"VK_F3"))
		return 0x72;
	if(dslisenum(vk,"VK_F4"))
		return 0x73;
	if(dslisenum(vk,"VK_F5"))
		return 0x74;
	if(dslisenum(vk,"VK_F6"))
		return 0x75;
	if(dslisenum(vk,"VK_F7"))
		return 0x76;
	if(dslisenum(vk,"VK_F8"))
		return 0x77;
	if(dslisenum(vk,"VK_F9"))
		return 0x78;
	if(dslisenum(vk,"VK_F10"))
		return 0x79;
	if(dslisenum(vk,"VK_F11"))
		return 0x7A;
	if(dslisenum(vk,"VK_F12"))
		return 0x7B;
	if(dslisenum(vk,"VK_F13"))
		return 0x7C;
	if(dslisenum(vk,"VK_F14"))
		return 0x7D;
	if(dslisenum(vk,"VK_F15"))
		return 0x7E;
	if(dslisenum(vk,"VK_F16"))
		return 0x7F;
	if(dslisenum(vk,"VK_F17"))
		return 0x80;
	if(dslisenum(vk,"VK_F18"))
		return 0x81;
	if(dslisenum(vk,"VK_F19"))
		return 0x82;
	if(dslisenum(vk,"VK_F20"))
		return 0x83;
	if(dslisenum(vk,"VK_F21"))
		return 0x84;
	if(dslisenum(vk,"VK_F22"))
		return 0x85;
	if(dslisenum(vk,"VK_F23"))
		return 0x86;
	if(dslisenum(vk,"VK_F24"))
		return 0x87;
	if(dslisenum(vk,"VK_NAVIGATION_VIEW"))
		return 0x88;
	if(dslisenum(vk,"VK_NAVIGATION_MENU"))
		return 0x89;
	if(dslisenum(vk,"VK_NAVIGATION_UP"))
		return 0x8A;
	if(dslisenum(vk,"VK_NAVIGATION_DOWN"))
		return 0x8B;
	if(dslisenum(vk,"VK_NAVIGATION_LEFT"))
		return 0x8C;
	if(dslisenum(vk,"VK_NAVIGATION_RIGHT"))
		return 0x8D;
	if(dslisenum(vk,"VK_NAVIGATION_ACCEPT"))
		return 0x8E;
	if(dslisenum(vk,"VK_NAVIGATION_CANCEL"))
		return 0x8F;
	if(dslisenum(vk,"VK_NUMLOCK"))
		return 0x90;
	if(dslisenum(vk,"VK_SCROLL"))
		return 0x91;
	if(dslisenum(vk,"VK_OEM_NEC_EQUAL"))
		return 0x92;
	if(dslisenum(vk,"VK_OEM_FJ_JISHO"))
		return 0x92;
	if(dslisenum(vk,"VK_OEM_FJ_MASSHOU"))
		return 0x93;
	if(dslisenum(vk,"VK_OEM_FJ_TOUROKU"))
		return 0x94;
	if(dslisenum(vk,"VK_OEM_FJ_LOYA"))
		return 0x95;
	if(dslisenum(vk,"VK_OEM_FJ_ROYA"))
		return 0x96;
	if(dslisenum(vk,"VK_LSHIFT"))
		return 0xA0;
	if(dslisenum(vk,"VK_RSHIFT"))
		return 0xA1;
	if(dslisenum(vk,"VK_LCONTROL"))
		return 0xA2;
	if(dslisenum(vk,"VK_RCONTROL"))
		return 0xA3;
	if(dslisenum(vk,"VK_LMENU"))
		return 0xA4;
	if(dslisenum(vk,"VK_RMENU"))
		return 0xA5;
	if(dslisenum(vk,"VK_BROWSER_BACK"))
		return 0xA6;
	if(dslisenum(vk,"VK_BROWSER_FORWARD"))
		return 0xA7;
	if(dslisenum(vk,"VK_BROWSER_REFRESH"))
		return 0xA8;
	if(dslisenum(vk,"VK_BROWSER_STOP"))
		return 0xA9;
	if(dslisenum(vk,"VK_BROWSER_SEARCH"))
		return 0xAA;
	if(dslisenum(vk,"VK_BROWSER_FAVORITES"))
		return 0xAB;
	if(dslisenum(vk,"VK_BROWSER_HOME"))
		return 0xAC;
	if(dslisenum(vk,"VK_VOLUME_MUTE"))
		return 0xAD;
	if(dslisenum(vk,"VK_VOLUME_DOWN"))
		return 0xAE;
	if(dslisenum(vk,"VK_VOLUME_UP"))
		return 0xAF;
	if(dslisenum(vk,"VK_MEDIA_NEXT_TRACK"))
		return 0xB0;
	if(dslisenum(vk,"VK_MEDIA_PREV_TRACK"))
		return 0xB1;
	if(dslisenum(vk,"VK_MEDIA_STOP"))
		return 0xB2;
	if(dslisenum(vk,"VK_MEDIA_PLAY_PAUSE"))
		return 0xB3;
	if(dslisenum(vk,"VK_LAUNCH_MAIL"))
		return 0xB4;
	if(dslisenum(vk,"VK_LAUNCH_MEDIA_SELECT"))
		return 0xB5;
	if(dslisenum(vk,"VK_LAUNCH_APP1"))
		return 0xB6;
	if(dslisenum(vk,"VK_LAUNCH_APP2"))
		return 0xB7;
	if(dslisenum(vk,"VK_OEM_1"))
		return 0xBA;
	if(dslisenum(vk,"VK_OEM_PLUS"))
		return 0xBB;
	if(dslisenum(vk,"VK_OEM_COMMA"))
		return 0xBC;
	if(dslisenum(vk,"VK_OEM_MINUS"))
		return 0xBD;
	if(dslisenum(vk,"VK_OEM_PERIOD"))
		return 0xBE;
	if(dslisenum(vk,"VK_OEM_2"))
		return 0xBF;
	if(dslisenum(vk,"VK_OEM_3"))
		return 0xC0;
	if(dslisenum(vk,"VK_GAMEPAD_A"))
		return 0xC3;
	if(dslisenum(vk,"VK_GAMEPAD_B"))
		return 0xC4;
	if(dslisenum(vk,"VK_GAMEPAD_X"))
		return 0xC5;
	if(dslisenum(vk,"VK_GAMEPAD_Y"))
		return 0xC6;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_SHOULDER"))
		return 0xC7;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_SHOULDER"))
		return 0xC8;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_TRIGGER"))
		return 0xC9;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_TRIGGER"))
		return 0xCA;
	if(dslisenum(vk,"VK_GAMEPAD_DPAD_UP"))
		return 0xCB;
	if(dslisenum(vk,"VK_GAMEPAD_DPAD_DOWN"))
		return 0xCC;
	if(dslisenum(vk,"VK_GAMEPAD_DPAD_LEFT"))
		return 0xCD;
	if(dslisenum(vk,"VK_GAMEPAD_DPAD_RIGHT"))
		return 0xCE;
	if(dslisenum(vk,"VK_GAMEPAD_MENU"))
		return 0xCF;
	if(dslisenum(vk,"VK_GAMEPAD_VIEW"))
		return 0xD0;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON"))
		return 0xD1;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON"))
		return 0xD2;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_THUMBSTICK_UP"))
		return 0xD3;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_THUMBSTICK_DOWN"))
		return 0xD4;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT"))
		return 0xD5;
	if(dslisenum(vk,"VK_GAMEPAD_LEFT_THUMBSTICK_LEFT"))
		return 0xD6;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_THUMBSTICK_UP"))
		return 0xD7;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN"))
		return 0xD8;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT"))
		return 0xD9;
	if(dslisenum(vk,"VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT"))
		return 0xDA;
	if(dslisenum(vk,"VK_OEM_4"))
		return 0xDB;
	if(dslisenum(vk,"VK_OEM_5"))
		return 0xDC;
	if(dslisenum(vk,"VK_OEM_6"))
		return 0xDD;
	if(dslisenum(vk,"VK_OEM_7"))
		return 0xDE;
	if(dslisenum(vk,"VK_OEM_8"))
		return 0xDF;
	if(dslisenum(vk,"VK_OEM_AX"))
		return 0xE1;
	if(dslisenum(vk,"VK_OEM_102"))
		return 0xE2;
	if(dslisenum(vk,"VK_ICO_HELP"))
		return 0xE3;
	if(dslisenum(vk,"VK_ICO_00"))
		return 0xE4;
	if(dslisenum(vk,"VK_PROCESSKEY"))
		return 0xE5;
	if(dslisenum(vk,"VK_ICO_CLEAR"))
		return 0xE6;
	if(dslisenum(vk,"VK_PACKET"))
		return 0xE7;
	if(dslisenum(vk,"VK_OEM_RESET"))
		return 0xE9;
	if(dslisenum(vk,"VK_OEM_JUMP"))
		return 0xEA;
	if(dslisenum(vk,"VK_OEM_PA1"))
		return 0xEB;
	if(dslisenum(vk,"VK_OEM_PA2"))
		return 0xEC;
	if(dslisenum(vk,"VK_OEM_PA3"))
		return 0xED;
	if(dslisenum(vk,"VK_OEM_WSCTRL"))
		return 0xEE;
	if(dslisenum(vk,"VK_OEM_CUSEL"))
		return 0xEF;
	if(dslisenum(vk,"VK_OEM_ATTN"))
		return 0xF0;
	if(dslisenum(vk,"VK_OEM_FINISH"))
		return 0xF1;
	if(dslisenum(vk,"VK_OEM_COPY"))
		return 0xF2;
	if(dslisenum(vk,"VK_OEM_AUTO"))
		return 0xF3;
	if(dslisenum(vk,"VK_OEM_ENLW"))
		return 0xF4;
	if(dslisenum(vk,"VK_OEM_BACKTAB"))
		return 0xF5;
	if(dslisenum(vk,"VK_ATTN"))
		return 0xF6;
	if(dslisenum(vk,"VK_CRSEL"))
		return 0xF7;
	if(dslisenum(vk,"VK_EXSEL"))
		return 0xF8;
	if(dslisenum(vk,"VK_EREOF"))
		return 0xF9;
	if(dslisenum(vk,"VK_PLAY"))
		return 0xFA;
	if(dslisenum(vk,"VK_ZOOM"))
		return 0xFB;
	if(dslisenum(vk,"VK_NONAME"))
		return 0xFC;
	if(dslisenum(vk,"VK_PA1"))
		return 0xFD;
	if(dslisenum(vk,"VK_OEM_CLEAR"))
		return 0xFE;
	return 0;
}
static int getLuaKey(lua_State *lua){ // uses first value on stack
	const char *name;
	int type;
	int key;
	
	type = lua_type(lua,1);
	if(type == LUA_TNUMBER)
		return lua_tonumber(lua,1);
	if(type != LUA_TSTRING)
		luaL_typerror(lua,1,"number or string");
	if(key = getDikFromString(name = lua_tostring(lua,1)))
		return key;
	if(key = getVkFromString(name))
		return MapVirtualKey(key,MAPVK_VK_TO_VSC);
	return 0;
}
static int getLuaKeySafe(lua_State *lua){
	int key;
	
	key = getLuaKey(lua);
	if(key <= 0 || key >= 0x100){
		lua_pushstring(lua,"unknown key: ");
		lua_pushvalue(lua,1);
		lua_concat(lua,2);
		luaL_argerror(lua,1,lua_tostring(lua,-1));
	}
	return key;
}

// OVERRIDE
static float getValueForButton(game_controller *controller,int binding,game_input *input){
	int a,b;
	
	// replica of 0x50C030
	switch(binding){
		case 256:
			if(input->mouse.rgbButtons[0])
				return 1.0f;
			break;
		case 257:
			if(input->mouse.rgbButtons[1])
				return 1.0f;
			break;
		case 258:
			if(input->mouse.rgbButtons[2])
				return 1.0f;
			break;
		case 259:
			if(input->mouse.rgbButtons[3])
				return 1.0f;
			break;
		case 260:
			return input->mouse.lZ;
		case 261:
			return -input->mouse.lZ;
		case 262:
			return -input->mouse.lY * getGameStickInputMultiplier();
		case 263:
			return input->mouse.lY * getGameStickInputMultiplier();
		case 264:
			return -input->mouse.lX * getGameStickInputMultiplier();
		case 265:
			return input->mouse.lX * getGameStickInputMultiplier();
	}
	if(binding < 0x100)
		return input->keyboard[binding] & 0x80 ? 1.0f : 0.0f;
	// missing but not needed?
	return 0.0f;
}
static float getValue(game_controller *controller,int button){
	// replica of 0x50C880
	if(controller->is_joy && button < 16)
		return getGameBindingsBasic()[button] & controller->input.joystick.buttons ? 1.0f : 0.0f;
	if(button < 16 || button == 24)
		return getValueForButton(controller,getGameBindingsAdvanced(0)[button],&controller->input) > 0.0f ? 1.0f : 0.0f;
	return ((float*)&controller->input.joystick)[button];
}
static void setValueForButton(game_controller *controller,int binding,game_input *input,float value){
	int a,b;
	
	switch(binding){
		case 256:
			input->mouse.rgbButtons[0] = value > 0.0f;
			break;
		case 257:
			input->mouse.rgbButtons[1] = value > 0.0f;
			break;
		case 258:
			input->mouse.rgbButtons[2] = value > 0.0f;
			break;
		case 259:
			input->mouse.rgbButtons[3] = value > 0.0f;
			break;
		case 260:
			input->mouse.lZ = value;
		case 261:
			input->mouse.lZ = -value;
		case 262:
			input->mouse.lY = -value / getGameStickInputMultiplier();
		case 263:
			input->mouse.lY = value / getGameStickInputMultiplier();
		case 264:
			input->mouse.lX = -value / getGameStickInputMultiplier();
		case 265:
			input->mouse.lX = value / getGameStickInputMultiplier();
	}
	if(binding < 0x100)
		input->keyboard[binding] = value > 0.0f ? 0x80 : 0;
	// missing but not needed?
}
static void setValue(game_controller *controller,int button,float value,int press){
	if(controller->is_joy && button < 16){
		button = getGameBindingsBasic()[button];
		if(value > 0.0f){
			if(press)
				controller->input.joystick.buttons |= button;
			if(~controller->last_input.joystick.buttons & button)
				controller->pressed |= button;
		}else{
			if(press)
				controller->input.joystick.buttons &= ~button;
			controller->pressed &= ~button;
		}
	}else if(button < 16 || button == 24)
		setValueForButton(controller,getGameBindingsAdvanced(0)[button],&controller->input,value);
	else
		((float*)&controller->input.joystick)[button] = value;
}
static int ZeroController(lua_State *lua){
	game_controller *controller;
	
	controller = getController(lua,1);
	memset(&controller->input,0,sizeof(game_input));
	memset(&controller->pressed,0,sizeof(int));
	return 0;
}
static int SetButtonPressed(lua_State *lua){
	int button;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	luaL_checktype(lua,3,LUA_TBOOLEAN);
	button = lua_tonumber(lua,1);
	if(button < 0 || button > 24)
		luaL_argerror(lua,1,"unsupported button");
	setValue(getController(lua,2),button,lua_toboolean(lua,3) ? 1.0f : 0.0f,1);
	return 0;
}
static int SetStickValue(lua_State *lua){
	int stick;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	luaL_checktype(lua,3,LUA_TNUMBER);
	stick = lua_tonumber(lua,1);
	if(stick < 0 || stick > 24)
		luaL_argerror(lua,1,"unsupported stick");
	setValue(getController(lua,2),stick,lua_tonumber(lua,3),0);
	return 0;
}

// HARDWARE
static int GetInputHardware(lua_State *lua){
	int button;
	int id;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	button = lua_tonumber(lua,1);
	if(button < 0 || button > 24)
		luaL_argerror(lua,1,"unsupported input");
	if(getController(lua,2)->is_joy){
		if(button < 16){
			button = getGameBindingsBasic()[button];
			lua_pushstring(lua,"button");
			for(id = 0;id < 16;id++)
				if(1 << id == button){
					lua_pushnumber(lua,id);
					return 2;
				}
			luaL_error(lua,"unbound input");
		}else if(button == 24){
			lua_pushstring(lua,"special");
			lua_pushnumber(lua,button-24);
		}else{
			lua_pushstring(lua,"analog");
			lua_pushnumber(lua,button-16);
		}
	}else if((button = getGameBindingsAdvanced(0)[button]) < 256){
		lua_pushstring(lua,"keyboard");
		lua_pushnumber(lua,button);
	}else if(button < 260){
		lua_pushstring(lua,"mouse_button");
		lua_pushnumber(lua,button-256);
	}else{
		lua_pushstring(lua,"mouse_movement");
		lua_pushnumber(lua,button-260);
	}
	return 2;
}
static int IsUsingJoystick(lua_State *lua){
	lua_pushboolean(lua,getController(lua,1)->is_joy);
	return 1;
}

// KEYBOARD
static int IsKeyValid(lua_State *lua){
	int key;
	int t;
	
	t = lua_type(lua,1);
	lua_pushboolean(lua,(t == LUA_TNUMBER || t == LUA_TSTRING) && (key = getLuaKey(lua)) > 0 && key < 0x100);
	return 1;
}
static int IsKeyPressed(lua_State *lua){
	game_controller *controller;
	dsl_state *dsl;
	int key;
	
	dsl = getDslState(lua,1);
	if(dsl->network && shouldNetworkDisableKeys(dsl->network)){
		lua_pushboolean(lua,0);
		return 0;
	}
	key = getLuaKeySafe(lua);
	if(lua_gettop(lua) >= 2 && !lua_isnil(lua,2)){
		controller = getController(lua,2);
		lua_pushboolean(lua,controller->input.keyboard[key]);
	}else
		lua_pushboolean(lua,dsl->keyboard[key] & DSL_KEY_PRESSED);
	return 1;
}
static int IsKeyBeingPressed(lua_State *lua){
	game_controller *controller;
	dsl_state *dsl;
	int key;
	
	dsl = getDslState(lua,1);
	if(dsl->network && shouldNetworkDisableKeys(dsl->network)){
		lua_pushboolean(lua,0);
		return 0;
	}
	key = getLuaKeySafe(lua);
	if(lua_gettop(lua) >= 2 && !lua_isnil(lua,2)){
		controller = getController(lua,2);
		lua_pushboolean(lua,controller->input.keyboard[key] && !controller->last_input.keyboard[key]);
	}else
		lua_pushboolean(lua,dsl->keyboard[key] == DSL_KEY_JUST_PRESSED);
	return 1;
}
static int IsKeyBeingReleased(lua_State *lua){
	game_controller *controller;
	dsl_state *dsl;
	int key;
	
	dsl = getDslState(lua,1);
	if(dsl->network && shouldNetworkDisableKeys(dsl->network)){
		lua_pushboolean(lua,0);
		return 0;
	}
	key = getLuaKeySafe(lua);
	if(lua_gettop(lua) >= 2 && !lua_isnil(lua,2)){
		controller = getController(lua,2);
		lua_pushboolean(lua,controller->last_input.keyboard[key] && !controller->input.keyboard[key]);
	}else
		lua_pushboolean(lua,dsl->keyboard[key] == DSL_KEY_JUST_RELEASED);
	return 1;
}
static int GetAnyKeyPressed(lua_State *lua){
	char *keyboard;
	int key;
	
	keyboard = getDslState(lua,1)->keyboard;
	for(key = 0;key < 0x100;key++)
		if(keyboard[key] & DSL_KEY_PRESSED){
			lua_pushnumber(lua,key);
			return 1;
		}
	return 0;
}
static int GetAnyKeyBeingPressed(lua_State *lua){
	char *keyboard;
	int key;
	
	keyboard = getDslState(lua,1)->keyboard;
	for(key = 0;key < 0x100;key++)
		if(keyboard[key] == DSL_KEY_JUST_PRESSED){
			lua_pushnumber(lua,key);
			return 1;
		}
	return 0;
}
static int GetAnyKeyBeingReleased(lua_State *lua){
	char *keyboard;
	int key;
	
	keyboard = getDslState(lua,1)->keyboard;
	for(key = 0;key < 0x100;key++)
		if(keyboard[key] == DSL_KEY_JUST_RELEASED){
			lua_pushnumber(lua,key);
			return 1;
		}
	return 0;
}

// TYPING
static int checkBuffer(typing_data *data){
	if(data->count == data->alloc){
		if(data->buffer || !(data->buffer = malloc(TYPING_BYTES)))
			return 0;
		data->alloc = TYPING_BYTES;
	}
	return 1;
}
static void pasteClipboard(typing_data *data,const char *clipboard){
	size_t count;
	size_t space;
	
	count = 0;
	while(isprint(clipboard[count]))
		count++;
	space = data->alloc - data->count;
	if(count < space){
		memcpy(data->buffer+data->count,clipboard,count);
		data->count += count;
	}else{
		memcpy(data->buffer+data->count,clipboard,space);
		data->count = data->alloc;
	}
}
static void processKeystroke(typing_data *data,int key){
	char keyboard[0x100];
	
	key = MapVirtualKey(key,MAPVK_VSC_TO_VK);
	if(!key || !GetKeyboardState(keyboard))
		return;
	if(key == VK_RETURN)
		data->status = STATUS_FINISHED;
	else if(key == VK_ESCAPE)
		data->status = STATUS_ABORTED;
	else if(key == VK_BACK){
		if(data->count)
			data->count--;
	}else if(key == 0x56 && keyboard[VK_CONTROL] & 0x80){
		if(OpenClipboard(NULL)){
			if(checkBuffer(data))
				pasteClipboard(data,GetClipboardData(CF_TEXT));
			CloseClipboard();
		}
	}else if(ToAscii(key,0,keyboard,(LPWORD)keyboard,0) == 1 && isprint(*keyboard) && checkBuffer(data))
		data->buffer[data->count++] = *keyboard;
}
static int keyDown(lua_State *lua,void *arg,int key){
	typing_data *data;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	data = lua_touserdata(lua,-1);
	lua_pop(lua,1);
	if(data && data->status == STATUS_TYPING)
		processKeystroke(data,key);
	return 0;
}
static int freeTyping(lua_State *lua){
	typing_data *data;
	
	data = lua_touserdata(lua,1);
	if(data->buffer)
		free(data->buffer);
	return 0;
}
static int StartTyping(lua_State *lua){
	typing_data *data;
	const char *str;
	
	if(lua_gettop(lua) >= 1){
		luaL_checktype(lua,1,LUA_TSTRING);
		str = lua_tostring(lua,1);
	}else
		str = NULL;
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	data = lua_touserdata(lua,-1);
	if(data && data->status == STATUS_TYPING){
		lua_pushboolean(lua,0);
		return 1;
	}
	lua_pop(lua,1);
	lua_pushstring(lua,REGISTRY_KEY);
	data = lua_newuserdata(lua,sizeof(typing_data));
	memset(data,0,sizeof(typing_data));
	lua_newtable(lua);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeTyping);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	if(str && (data->buffer = malloc(TYPING_BYTES))){
		data->alloc = TYPING_BYTES;
		strncpy(data->buffer,str,TYPING_BYTES);
		data->count = strlen(data->buffer);
	}
	lua_rawset(lua,LUA_REGISTRYINDEX);
	lua_pushboolean(lua,1);
	return 1;
}
static int IsTypingActive(lua_State *lua){
	typing_data *data;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	data = lua_touserdata(lua,-1);
	lua_pushboolean(lua,data && data->status == STATUS_TYPING);
	return 1;
}
static int GetTypingString(lua_State *lua){
	typing_data *data;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	if(data = lua_touserdata(lua,-1)){
		if(data->buffer)
			lua_pushlstring(lua,data->buffer,data->count);
		else
			lua_pushstring(lua,"");
		return 1;
	}
	return 0;
}
static int WasTypingAborted(lua_State *lua){
	typing_data *data;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	data = lua_touserdata(lua,-1);
	lua_pushboolean(lua,data && data->status == STATUS_ABORTED);
	return 1;
}
static int StopTyping(lua_State *lua){
	typing_data *data;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	data = lua_touserdata(lua,-1);
	if(data)
		data->status = STATUS_ABORTED;
	return 0;
}

// MOUSE
static int GetMouseInput(lua_State *lua){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	lua_pushnumber(lua,dsl->mouse.lX);
	lua_pushnumber(lua,dsl->mouse.lY);
	return 2;
}
static int GetMouseScroll(lua_State *lua){
	lua_pushnumber(lua,getDslState(lua,1)->mouse.lZ);
	return 1;
}
static int IsMousePressed(lua_State *lua){
	int index;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	index = lua_tonumber(lua,1);
	if(index < 0 || index > 3)
		luaL_argerror(lua,1,"invalid mouse button");
	lua_pushboolean(lua,getDslState(lua,1)->mouse.rgbButtons[index] & 0x80);
	return 1;
}
static int IsMouseBeingPressed(lua_State *lua){
	dsl_state *dsl;
	int index;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	index = lua_tonumber(lua,1);
	if(index < 0 || index > 3)
		luaL_argerror(lua,1,"invalid mouse button");
	dsl = getDslState(lua,1);
	lua_pushboolean(lua,dsl->mouse.rgbButtons[index] & 0x80 && ~dsl->mouse2.rgbButtons[index] & 0x80);
	return 1;
}
static int IsMouseBeingReleased(lua_State *lua){
	dsl_state *dsl;
	int index;
	
	luaL_checktype(lua,1,LUA_TNUMBER);
	index = lua_tonumber(lua,1);
	if(index < 0 || index > 3)
		luaL_argerror(lua,1,"invalid mouse button");
	dsl = getDslState(lua,1);
	lua_pushboolean(lua,~dsl->mouse.rgbButtons[index] & 0x80 && dsl->mouse2.rgbButtons[index] & 0x80);
	return 1;
}

// DEBUG
static int GetStickValue2(lua_State *lua){
	luaL_checktype(lua,1,LUA_TNUMBER);
	lua_pushnumber(lua,getValue(getController(lua,2),lua_tonumber(lua,1)));
	return 1;
}
static int PrintInputDebug(lua_State *lua){
	script_console *console;
	
	console = getDslState(lua,1)->console;
	printConsoleOutput(console,"sizeof(DIJOYSTATE) = 0x%zX",sizeof(DIJOYSTATE));
	printConsoleOutput(console,"sizeof(DIMOUSESTATE) = 0x%zX",sizeof(DIMOUSESTATE));
	printConsoleOutput(console,"sizeof(game_joy) = 0x%zX",sizeof(game_joy));
	printConsoleOutput(console,"sizeof(game_input) = 0x%zX",sizeof(game_input));
	printConsoleOutput(console,"sizeof(game_controller) = 0x%zX",sizeof(game_controller));
	printConsoleOutput(console,"offsetof(game_controller,input) = 0x%zX",offsetof(game_controller,input));
	printConsoleOutput(console,"offsetof(game_controller,last_input) = 0x%zX",offsetof(game_controller,last_input));
	return 0;
}

// OPEN
int dslopen_input(lua_State *lua){
	dsl_state *dsl;
	
	dsl = getDslState(lua,1);
	addScriptEventCallback(dsl->events,EVENT_KEY_DOWN_UPDATE,(script_event_cb)&keyDown,NULL);
	// override
	lua_register(lua,"ZeroController",&ZeroController);
	lua_register(lua,"SetButtonPressed",&SetButtonPressed);
	lua_register(lua,"SetStickValue",&SetStickValue);
	// hardware
	lua_register(lua,"GetInputHardware",&GetInputHardware);
	lua_register(lua,"IsUsingJoystick",&IsUsingJoystick);
	// keyboard
	lua_register(lua,"IsKeyValid",&IsKeyValid); // boolean (number|string key)
	lua_register(lua,"IsKeyPressed",&IsKeyPressed); // boolean (number|string key)
	lua_register(lua,"IsKeyBeingPressed",&IsKeyBeingPressed); // boolean (number|string key)
	lua_register(lua,"IsKeyBeingReleased",&IsKeyBeingReleased); // boolean (number|string key)
	lua_register(lua,"GetAnyKeyPressed",&GetAnyKeyPressed); // number|none ()
	lua_register(lua,"GetAnyKeyBeingPressed",&GetAnyKeyBeingPressed); // number|none ()
	lua_register(lua,"GetAnyKeyBeingReleased",&GetAnyKeyBeingReleased); // number|none ()
	// typing
	lua_register(lua,"StartTyping",&StartTyping); // boolean ()
	lua_register(lua,"IsTypingActive",&IsTypingActive); // boolean ()
	lua_register(lua,"GetTypingString",&GetTypingString); // string|none ()
	lua_register(lua,"WasTypingAborted",&WasTypingAborted); // boolean ()
	lua_register(lua,"StopTyping",&StopTyping); // none ()
	// mouse
	lua_register(lua,"GetMouseInput",&GetMouseInput); // number, number ()
	lua_register(lua,"GetMouseScroll",&GetMouseScroll);
	lua_register(lua,"IsMousePressed",&IsMousePressed);
	lua_register(lua,"IsMouseBeingPressed",&IsMouseBeingPressed);
	lua_register(lua,"IsMouseBeingReleased",&IsMouseBeingReleased);
	// debug
	if(dsl->flags & DSL_ADD_DEBUG_FUNCS){
		lua_register(lua,"GetStickValue2",&GetStickValue2);
		lua_register(lua,"PrintInputDebug",&PrintInputDebug);
	}
	return 0;
}