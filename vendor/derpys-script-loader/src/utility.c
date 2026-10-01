/* DERPY'S SCRIPT LOADER: UTILITY
	
	a few general purpose DSL functions
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#include <sys/stat.h> // mkdir
#define MAX_PATH PATH_MAX
#endif

#define RENDER_TARGET_NAME "render_target"
//#define DEBUG_SAFE_PATH

// GET DSL
dsl_state* getDslState(lua_State *lua,int unprotected){
	dsl_state *result;
	
	lua_pushstring(lua,DSL_REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	result = lua_touserdata(lua,-1);
	lua_pop(lua,1);
	if(unprotected && !result)
		luaL_error(lua,"corrupted dsl state");
	return result;
}

// FILE SAFETY
static int isFileInPath2(const char *name,const char *path){
	char buffer[MAX_PATH];
	size_t count;
	
	#ifdef _WIN32
	count = GetFullPathName(path,MAX_PATH,buffer,NULL);
	if(!count || count >= MAX_PATH || buffer[count-1] != '\\')
		return 0;
	#else
	if(!realpath(path,buffer))
		return 0;
	count = strlen(buffer);
	if(!count)
		return 0;
	if(buffer[count-1] != '/'){
		if(count + 1 >= MAX_PATH)
			return 0;
		buffer[count] = '/';
		buffer[++count] = 0;
	}
	#endif
	return strlen(name) > count && !strncmp(name,buffer,count);
}
#ifdef DEBUG_SAFE_PATH
static int isFileInPath(const char *name,const char *path){
	FILE *file;
	int result;
	
	result = isFileInPath2(name,path);
	if(file = fopen("safepathdebug.txt","a")){
		fprintf(file,"isFileInPath2(\"%s\",\"%s\") = %d\n",name,path,result);
		fclose(file);
	}
	return result;
}
#else
#define isFileInPath(name,path) isFileInPath2(name,path)
#endif
static int isFileSafe(const char *name){
	return isFileInPath(name,DSL_SCRIPTS_PATH)
		#ifndef DSL_SERVER_VERSION
		|| isFileInPath(name,DSL_SERVER_SCRIPTS_PATH)
		#endif
		|| isFileInPath(name,DSL_PACKAGE_PATH);
}
int isDslFileSafe(dsl_state *dsl,const unsigned char *path){
	char buffer[MAX_PATH];
	size_t i;
	char c;
	
	if(dsl->flags & DSL_SYSTEM_ACCESS)
		return 1; // no matter what, system access will allow anything.
	if(strlen(path) >= MAX_PATH)
		return 0; // >= because MAX_PATH includes NUL.
	for(i = 0;c = path[i];i++)
		#ifdef _WIN32
		if(c < 0x20 || c >= 0x7F || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
		#else
		// stricter names on linux because i'm not as familiar with the rules
		if(!(c == ' ' || c == '-' || c == '_' || c == '/' || c == '\\' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' || c <= 'z')))
		#endif
			return 0;
	#ifdef _WIN32
	i = GetFullPathName(path,MAX_PATH,buffer,NULL);
	if(!i || i >= MAX_PATH)
		return 0;
	#else
	if(!realpath(path,buffer))
		return 0;
	i = strlen(buffer);
	#endif
	while(i-- && (buffer[i] == '/' || buffer[i] == '\\')) // trim trailing slashes
		buffer[i] = 0;
	return isFileSafe(buffer);
}

// PATH CREATION
int checkDslPathExists(const char *path,int skiplast){
	char buffer[MAX_PATH];
	char *iter;
	
	if(strlen(path) >= MAX_PATH)
		return 0;
	strcpy(buffer,path);
	if(*buffer){
		for(iter = buffer;iter[1];iter++)
			if(*iter == '/' || *iter == '\\'){
				*iter = 0;
				#ifdef _WIN32
				if(!CreateDirectory(buffer,NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
				#else
				if(mkdir(buffer,S_IRUSR|S_IWUSR) && errno != EEXIST)
				#endif
					return 0;
				#ifdef _WIN32
				*iter = '\\';
				#else
				*iter = '/';
				#endif
			}
		if(*iter == '/' || *iter == '\\')
			*iter = 0;
	}
	#ifdef _WIN32
	return skiplast || CreateDirectory(buffer,NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
	#else
	return skiplast || !mkdir(buffer,S_IRUSR|S_IWUSR) || errno == EEXIST;
	#endif
}

// PRINT PREFIX
const char* getDslPrintPrefix(dsl_state *dsl,int include_thread){
	static char buffer[64];
	script_manager *sm;
	const char *name;
	thread *t;
	void *game;
	int index;
	
	sm = dsl->manager;
	if(sm->running_collection){
		strcpy(buffer,"["); // 1
		strncat(buffer,getScriptCollectionName(sm->running_collection),29); // up to 30
		if(include_thread && (t = sm->running_thread) && (name = getThreadName(t))){
			strcat(buffer,", "); // up to 32
			strncat(buffer,name,29); // up to 61
		}
		strcat(buffer,"] "); // up to 63 + NUL
	#ifndef DSL_SERVER_VERSION
	}else if((game = dsl->game) && (index = getGameScriptIndex(game)) != -1 && getGameScriptCount(game)){
		name = getGameScriptPool(game)[index];
		strcpy(buffer,"["); // 1
		strncat(buffer,name+4,60); // up to 61
		strcat(buffer,"] "); // up to 63 + NUL
	#endif
	}else
		*buffer = 0;
	return buffer;
}

// GENERAL EVENTS
#ifndef DSL_SERVER_VERSION
static struct hwnd_style{
	const char *name;
	LONG value;
}hwnd_styles[] = {
	{"WS_BORDER",WS_BORDER},
	{"WS_CAPTION",WS_CAPTION},
	{"WS_CHILD",WS_CHILD},
	{"WS_CHILDWINDOW",WS_CHILDWINDOW},
	{"WS_CLIPCHILDREN",WS_CLIPCHILDREN},
	{"WS_CLIPSIBLINGS",WS_CLIPSIBLINGS},
	{"WS_DISABLED",WS_DISABLED},
	{"WS_DLGFRAME",WS_DLGFRAME},
	{"WS_GROUP",WS_GROUP},
	{"WS_HSCROLL",WS_HSCROLL},
	{"WS_ICONIC",WS_ICONIC},
	{"WS_MAXIMIZE",WS_MAXIMIZE},
	{"WS_MAXIMIZEBOX",WS_MAXIMIZEBOX},
	{"WS_MINIMIZE",WS_MINIMIZE},
	{"WS_MINIMIZEBOX",WS_MINIMIZEBOX},
	{"WS_OVERLAPPED",WS_OVERLAPPED},
	{"WS_OVERLAPPEDWINDOW",WS_OVERLAPPEDWINDOW},
	{"WS_POPUP",WS_POPUP},
	{"WS_POPUPWINDOW",WS_POPUPWINDOW},
	{"WS_SIZEBOX",WS_SIZEBOX},
	{"WS_SYSMENU",WS_SYSMENU},
	{"WS_TABSTOP",WS_TABSTOP},
	{"WS_THICKFRAME",WS_THICKFRAME},
	{"WS_TILED",WS_TILED},
	{"WS_TILEDWINDOW",WS_TILEDWINDOW},
	{"WS_VISIBLE",WS_VISIBLE},
	{"WS_VSCROLL",WS_VSCROLL}
};
void runDslWindowStyleEvent(dsl_state *dsl,HWND window,int show){
	LONG style;
	RECT rect;
	lua_State *lua;
	int length;
	char *text;
	int i,n;
	
	style = GetWindowLong(window,GWL_STYLE);
	GetClientRect(window,&rect);
	lua = dsl->lua;
	length = GetWindowTextLength(window);
	text = malloc(length+1);
	n = sizeof(hwnd_styles)/sizeof(struct hwnd_style);
	lua_newtable(lua);
	lua_pushstring(lua,"x");
	lua_pushnumber(lua,rect.left);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"y");
	lua_pushnumber(lua,rect.top);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"width");
	lua_pushnumber(lua,rect.right - rect.left);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"height");
	lua_pushnumber(lua,rect.bottom - rect.top);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"title");
	if(text && GetWindowText(window,text,length+1))
		lua_pushlstring(lua,text,length);
	else
		lua_pushstring(lua,"");
	lua_rawset(lua,-3);
	lua_pushstring(lua,"style");
	lua_newtable(lua);
	for(i = 0;i < n;i++){
		lua_pushstring(lua,hwnd_styles[i].name);
		lua_pushboolean(lua,(style & hwnd_styles[i].value) == hwnd_styles[i].value);
		lua_rawset(lua,-3);
	}
	lua_rawset(lua,-3);
	if(runLuaScriptEvent(dsl->events,lua,LOCAL_EVENT,"WindowUpdating",1)){
		lua_pop(lua,1); // no event handlers returned true, so we won't bother anything
		if(text)
			free(text);
		return;
	}
	lua_pushstring(lua,"style");
	lua_rawget(lua,-2);
	if(lua_istable(lua,-1)){
		style = 0;
		for(i = 0;i < n;i++){
			lua_pushstring(lua,hwnd_styles[i].name);
			lua_rawget(lua,-2);
			if(lua_toboolean(lua,-1))
				style |= hwnd_styles[i].value;
			lua_pop(lua,1);
		}
		SetWindowLong(window,GWL_STYLE,style);
	}
	lua_pop(lua,1); // style table
	if(text){
		lua_pushstring(lua,"title");
		lua_rawget(lua,-2);
		if(lua_isstring(lua,-1))
			SetWindowText(window,lua_tostring(lua,-1));
		lua_pop(lua,1); // title
		free(text);
	}
	lua_pushstring(lua,"x");
	lua_rawget(lua,-2);
	lua_pushstring(lua,"y");
	lua_rawget(lua,-3);
	lua_pushstring(lua,"width");
	lua_rawget(lua,-4);
	lua_pushstring(lua,"height");
	lua_rawget(lua,-5);
	rect.left = lua_tonumber(lua,-4);
	rect.top = lua_tonumber(lua,-3);
	rect.right = rect.left + lua_tonumber(lua,-2);
	rect.bottom = rect.top + lua_tonumber(lua,-1);
	AdjustWindowRect(&rect,style,0);
	SetWindowPos(window,0,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
	lua_pop(lua,5); // table and position stuff
	if(show)
		ShowWindow(window,SW_SHOW);
}
#endif

// LUA TEXTURES
#ifndef DSL_SERVER_VERSION
static int getLuaTextureString(lua_State *lua){
	lua_pushfstring(lua,"texture: %s",(char*)((lua_texture*)lua_touserdata(lua,1)+1));
	return 1;
}
static int freeLuaTexture(lua_State *lua){
	lua_texture *lt;
	
	lt = lua_touserdata(lua,1);
	if(lt->texture)
		destroyRenderTexture(lt->render,lt->texture);
	return 0;
}
void createLuaTexture(lua_State *lua,dsl_state *dsl,render_state *render,const char *path,int width,int height){ // pushes userdata
	lua_texture *lt;
	dsl_file *png;
	
	lua_setgcthreshold(lua,0); // collect any existing textures to save video memory
	if(path){
		lt = lua_newuserdata(lua,sizeof(lua_texture) + strlen(path) + 1);
		strcpy((char*)(lt+1),path);
	}else{
		lt = lua_newuserdata(lua,sizeof(lua_texture) + strlen(RENDER_TARGET_NAME) + 1);
		strcpy((char*)(lt+1),RENDER_TARGET_NAME);
	}
	lt->texture = NULL;
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getLuaTextureString);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeLuaTexture);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	if(path){
		png = openDslFile(dsl,NULL,path,"rb",NULL);
		if(!png)
			luaL_error(lua,lua_pushfstring(lua,"failed to create texture (%s: %s)",getDslFileError(),path));
		lt->texture = createRenderTexture2(render,png);
		closeDslFile(png);
	}else
		lt->texture = createRenderTextureTarget(render,width,height);
	if(!lt->texture)
		luaL_error(lua,"%s",getRenderErrorString(render));
	lt->render = render;
	lt->x1 = 0.0f;
	lt->y1 = 0.0f;
	lt->x2 = 1.0f;
	lt->y2 = 1.0f;
}
void createLuaTexture2(lua_State *lua,render_state *render,const char *name,const char *path){ // pushes userdata
	lua_texture *lt;
	FILE *file;
	
	lua_setgcthreshold(lua,0); // collect any existing textures
	lt = lua_newuserdata(lua,sizeof(lua_texture) + strlen(name) + 1);
	lt->texture = NULL;
	strcpy((char*)(lt+1),name);
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getLuaTextureString);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeLuaTexture);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	file = fopen(path,"rb");
	if(!path)
		luaL_error(lua,lua_pushfstring(lua,"failed to create texture (%s: %s)",strerror(errno),path));
	lt->texture = createRenderTexture(render,file);
	fclose(file);
	if(!lt->texture)
		luaL_error(lua,"%s",getRenderErrorString(render));
	lt->render = render;
	lt->x1 = 0.0f;
	lt->y1 = 0.0f;
	lt->x2 = 1.0f;
	lt->y2 = 1.0f;
}
#endif

// STRING COMPARISON
int dslisenum(const char *a,const char *b){
	for(;*a || *b;a++,b++)
		if(toupper(*a) != *b)
			return 0;
	return 1;
}
int dslstrcmp(const char *a,const char *b){
	int cmp;
	
	for(;*a || *b;a++,b++)
		if(cmp = (tolower(*a) - tolower(*b)))
			return cmp;
	return 0;
}
int dslstrncmp(const char *a,const char *b,int n){
	int cmp;
	
	for(;n && (*a || *b);a++,b++,n--)
		if(cmp = (tolower(*a) - tolower(*b)))
			return cmp;
	return 0;
}