// * DERPY'S SCRIPT LOADER: RENDER LIBRARY

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>

#define REGISTRY_KEY DSL_REGISTRY_KEY "_render"
#define DEFAULT_FONT "Arial"
#define DEFAULT_SIZE 0.025f // 36 pt on 1080p
#define LIMIT_TEXT_SIZE 4.0f // *
#define LOWER_TEXT_WRAP 0.01f // *
#define MAX_CACHE_DRAWS 1024 // *
#define MAX_INLINE_LINES 32
#define RADIANS_PER_DEGREE (3.14159265358979323846/180.0)
#define MAX_CUSTOM_WIDTH 3840
#define MAX_CUSTOM_HEIGHT 2160

// LAYERS
#define CACHE_PRE_FADE 0
#define CACHE_POST_WORLD 1
#define CACHE_POST_FADE 2
#define CACHE_TOTAL_COUNT 3

// MAIN TYPES
typedef struct font_cache{
	struct font_cache *next;
	int font;
}font_cache; // name follows
typedef struct lua_cache{
	int count;
	int table;
}lua_cache;
typedef struct lua_render{
	render_state *render;
	int cached; // if the current draw was cached
	lua_cache caches[CACHE_TOTAL_COUNT];
	lua_cache *cache;
	font_cache *fonts; // named fonts for SetTextFont if a created font is not used
	int font;
	int inlines; // table for inline text [style] = inline_state
	int textsetup; // if textargs have been setup, otherwise it need defaults set
	render_text_draw textargs;
}lua_render;
typedef struct lua_font{
	render_state *render;
	render_font *font;
}lua_font;

// LUA RENDER STATE
static int freeLuaRender(lua_State *lua){
	lua_render *lr;
	font_cache *fc,*nfc;
	int ref;
	int i;
	
	lr = lua_touserdata(lua,1);
	for(fc = lr->fonts;fc;fc = nfc){
		nfc = fc->next;
		free(fc);
	}
	return 0;
}
static lua_render* setupLuaRender(lua_State *lua,dsl_state *dsl){
	lua_render *lr;
	int i;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lr = lua_newuserdata(lua,sizeof(lua_render));
	memset(lr,0,sizeof(lua_render));
	lr->render = dsl->render;
	lr->cache = lr->caches;
	lr->font = LUA_NOREF;
	lua_newtable(lua);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeLuaRender);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lua_rawset(lua,LUA_REGISTRYINDEX);
	for(i = 0;i < CACHE_TOTAL_COUNT;i++){
		lua_newtable(lua);
		lr->caches[i].table = luaL_ref(lua,LUA_REGISTRYINDEX);
	}
	lua_newtable(lua);
	lua_newtable(lua);
	lua_pushstring(lua,"__mode");
	lua_pushstring(lua,"v"); // don't need to keep inlines if they're not in the cache
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	lr->inlines = luaL_ref(lua,LUA_REGISTRYINDEX);
	return lr;
}
static lua_render* getLuaRender(lua_State *lua){
	lua_render *lr;
	
	lua_pushstring(lua,REGISTRY_KEY);
	lua_rawget(lua,LUA_REGISTRYINDEX);
	lr = lua_touserdata(lua,-1);
	if(!lr->render)
		luaL_error(lua,"invalid renderer"); // just during the pre-init stage
	lua_pop(lua,1);
	return lr;
}

// BASIC UTILITY
static DWORD getLuaRgb(lua_State *lua,int index){
	return (DWORD)lua_tonumber(lua,index) << 16 | (DWORD)lua_tonumber(lua,index+1) << 8 | (DWORD)lua_tonumber(lua,index+2);
}
static DWORD getLuaRgba(lua_State *lua,int index){
	return (DWORD)lua_tonumber(lua,index) << 16 | (DWORD)lua_tonumber(lua,index+1) << 8 | (DWORD)lua_tonumber(lua,index+2) | (DWORD)lua_tonumber(lua,index+3) << 24;
}

// CACHE UTILITY
static void addCacheCall(lua_State *lua,lua_cache *lc){
	lua_Debug ar;
	int i,n;
	
	#ifdef MAX_CACHE_DRAWS
	if(lc->count >= MAX_CACHE_DRAWS)
		return;
	#endif
	n = lua_gettop(lua);
	lua_rawgeti(lua,LUA_REGISTRYINDEX,lc->table);
	lua_newtable(lua);
	if(!lua_getstack(lua,0,&ar) || !lua_getinfo(lua,"f",&ar)){
		lua_pop(lua,2);
		return;
	}
	lua_rawseti(lua,-2,1); // function
	i = 1;
	while(i <= n){
		lua_pushvalue(lua,i);
		lua_rawseti(lua,-2,++i);
	}
	lua_rawseti(lua,-2,++lc->count);
	lua_pop(lua,1);
}
static void runCacheCalls(lua_State *lua,lua_render *lr,lua_cache *lc){
	int i,n,arg,call;
	dsl_state *dsl;
	
	n = lc->count;
	if(!n)
		return;
	lc->count = 0;
	lr->cache = lc; // because cached draws can cache new draws
	lr->cached = 1;
	lua_rawgeti(lua,LUA_REGISTRYINDEX,lc->table);
	for(i = 1;i <= n;i++){
		lua_rawgeti(lua,-1,i);
		call = lua_gettop(lua);
		lua_rawgeti(lua,call,1); // get function
		for(arg = 2;lua_rawgeti(lua,call,arg),!lua_isnil(lua,-1);arg++) // get arguments
			if(!lua_checkstack(lua,LUA_MINSTACK))
				break; // probably won't happen so fuck it just don't push more args?
		lua_pop(lua,1); // pop nil
		if(lua_pcall(lua,arg-2,0,0)){
			if(dsl = getDslState(lua,0)){
				if(lua_isstring(lua,-1))
					printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),lua_tostring(lua,-1));
				else
					printConsoleError(dsl->console,"%s%s",getDslPrintPrefix(dsl,1),TEXT_FAIL_UNKNOWN);
			}
			lua_pop(lua,1); // pop error value
		}
		lua_pop(lua,1); // pop call table
	}
	for(i = lc->count;n > i;n--){ // erase all old cache draws
		lua_pushnil(lua);
		lua_rawseti(lua,-2,n);
	}
	lua_pop(lua,1); // pop cache table
	lr->cached = 0;
}
static render_state* getRenderOrCache(lua_State *lua,lua_render *lr){
	render_state *render;
	
	if(isRenderActive(render = lr->render))
		return render;
	addCacheCall(lua,lr->cache);
	return NULL;
}

// DISPLAY
static int dsl_GetDisplayResolution(lua_State *lua){
	int w,h;
	
	getRenderResolution(getLuaRender(lua)->render,&w,&h);
	lua_pushnumber(lua,w);
	lua_pushnumber(lua,h);
	return 2;
}
static int dsl_GetDisplayAspectRatio(lua_State *lua){
	lua_pushnumber(lua,getRenderAspect(getLuaRender(lua)->render));
	return 1;
}

// LAYER
static int dsl_SetDrawLayer(lua_State *lua){
	lua_render *lr;
	const char *id;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	id = lua_tostring(lua,1);
	lr = getLuaRender(lua);
	if(dslisenum(id,"POST_WORLD"))
		lr->cache = lr->caches + CACHE_POST_WORLD;
	else if(dslisenum(id,"PRE_FADE"))
		lr->cache = lr->caches + CACHE_PRE_FADE;
	else if(dslisenum(id,"POST_FADE"))
		lr->cache = lr->caches + CACHE_POST_FADE;
	else{
		lua_pushstring(lua,"unknown layer: ");
		lua_pushvalue(lua,1);
		lua_concat(lua,2);
		luaL_argerror(lua,1,lua_tostring(lua,-1));
	}
	return 0;
}

// BASIC
static int dsl_DrawRectangle(lua_State *lua){
	render_state *render;
	int arg;
	
	for(arg = 1;arg <= 4;arg++)
		luaL_checktype(lua,arg,LUA_TNUMBER);
	while(lua_gettop(lua) < 8)
		lua_pushnumber(lua,255);
	render = getRenderOrCache(lua,getLuaRender(lua));
	if(render && !drawRenderRectangle(render,lua_tonumber(lua,1),lua_tonumber(lua,2),lua_tonumber(lua,3),lua_tonumber(lua,4),getLuaRgba(lua,5)))
		luaL_error(lua,"%s",getRenderErrorString(render));
	return 0;
}

// TEXTURE
static int dsl_CreateTexture(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	createLuaTexture(lua,getDslState(lua,1),getLuaRender(lua)->render,lua_tostring(lua,-1),0,0);
	return 1;
}
static int dsl_DrawTexture(lua_State *lua){
	render_state *render;
	lua_texture *lt;
	int arg;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	for(arg = 2;arg <= 5;arg++)
		luaL_checktype(lua,arg,LUA_TNUMBER);
	while(lua_gettop(lua) < 9)
		lua_pushnumber(lua,255);
	if(!isRenderActive(render = lt->render)){
		lua_settop(lua,9);
		lua_pushnumber(lua,lt->x1);
		lua_pushnumber(lua,lt->y1);
		lua_pushnumber(lua,lt->x2);
		lua_pushnumber(lua,lt->y2);
		addCacheCall(lua,getLuaRender(lua)->cache);
	}else if(getLuaRender(lua)->cached){
		if(!drawRenderTextureUV(render,lt->texture,lua_tonumber(lua,2),lua_tonumber(lua,3),lua_tonumber(lua,4),lua_tonumber(lua,5),getLuaRgba(lua,6),lua_tonumber(lua,10),lua_tonumber(lua,11),lua_tonumber(lua,12),lua_tonumber(lua,13)))
			luaL_error(lua,"%s",getRenderErrorString(render));
	}else if(!drawRenderTextureUV(render,lt->texture,lua_tonumber(lua,2),lua_tonumber(lua,3),lua_tonumber(lua,4),lua_tonumber(lua,5),getLuaRgba(lua,6),lt->x1,lt->y1,lt->x2,lt->y2))
		luaL_error(lua,"%s",getRenderErrorString(render));
	return 0;
}
static int dsl_DrawTexture2(lua_State *lua){
	render_state *render;
	lua_texture *lt;
	int arg;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	for(arg = 2;arg <= 6;arg++)
		luaL_checktype(lua,arg,LUA_TNUMBER);
	while(lua_gettop(lua) < 10)
		lua_pushnumber(lua,255);
	if(!isRenderActive(render = lt->render)){
		lua_settop(lua,10);
		lua_pushnumber(lua,lt->x1);
		lua_pushnumber(lua,lt->y1);
		lua_pushnumber(lua,lt->x2);
		lua_pushnumber(lua,lt->y2);
		addCacheCall(lua,getLuaRender(lua)->cache);
	}else if(getLuaRender(lua)->cached){
		if(!drawRenderTextureUV2(render,lt->texture,lua_tonumber(lua,2),lua_tonumber(lua,3),lua_tonumber(lua,4),lua_tonumber(lua,5),getLuaRgba(lua,7),lua_tonumber(lua,11),lua_tonumber(lua,12),lua_tonumber(lua,13),lua_tonumber(lua,14),lua_tonumber(lua,6)*RADIANS_PER_DEGREE))
			luaL_error(lua,"%s",getRenderErrorString(render));
	}else if(!drawRenderTextureUV2(render,lt->texture,lua_tonumber(lua,2),lua_tonumber(lua,3),lua_tonumber(lua,4),lua_tonumber(lua,5),getLuaRgba(lua,7),lt->x1,lt->y1,lt->x2,lt->y2,lua_tonumber(lua,6)*RADIANS_PER_DEGREE))
		luaL_error(lua,"%s",getRenderErrorString(render));
	return 0;
}
static int dsl_SetTextureBounds(lua_State *lua){
	lua_texture *lt;
	int arg;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	for(arg = 2;arg <= 5;arg++)
		luaL_checktype(lua,arg,LUA_TNUMBER);
	lt->x1 = lua_tonumber(lua,2);
	lt->y1 = lua_tonumber(lua,3);
	lt->x2 = lua_tonumber(lua,4);
	lt->y2 = lua_tonumber(lua,5);
	return 0;
}
static int dsl_GetTextureResolution(lua_State *lua){
	lua_texture *lt;
	int w,h;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	getRenderTextureResolution(lt->render,lt->texture,&w,&h);
	lua_pushnumber(lua,w);
	lua_pushnumber(lua,h);
	return 2;
}
static int dsl_GetTextureAspectRatio(lua_State *lua){
	lua_texture *lt;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	lua_pushnumber(lua,getRenderTextureAspect(lt->render,lt->texture));
	return 1;
}
static int dsl_GetTextureDisplayAspectRatio(lua_State *lua){
	lua_texture *lt;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	lua_pushnumber(lua,getRenderTextureAspect(lt->render,lt->texture)/getRenderAspect(lt->render));
	return 1;
}

// FONT UTILITY
static int getLuaFontString(lua_State *lua){
	lua_pushfstring(lua,"font: %s",(char*)((lua_font*)lua_touserdata(lua,1)+1));
	return 1;
}
static int freeLuaFont(lua_State *lua){
	lua_font *lf;
	
	lf = lua_touserdata(lua,1);
	if(lf->font)
		destroyRenderFont(lf->render,lf->font);
	return 0;
}
static render_font* createLuaFont(lua_State *lua,render_state *render,const char *name){ // pushes userdata
	render_font *font;
	lua_font *lf;
	
	lua_setgcthreshold(lua,0); // collect any unreferenced font objects
	lf = lua_newuserdata(lua,sizeof(lua_font) + strlen(name) + 1);
	lf->font = NULL;
	strcpy((char*)(lf+1),name);
	lua_newtable(lua);
	lua_pushstring(lua,"__tostring");
	lua_pushcfunction(lua,&getLuaFontString);
	lua_rawset(lua,-3);
	lua_pushstring(lua,"__gc");
	lua_pushcfunction(lua,&freeLuaFont);
	lua_rawset(lua,-3);
	lua_setmetatable(lua,-2);
	font = createRenderFont(render,NULL,name);
	if(!font)
		luaL_error(lua,"%s",getRenderErrorString(render));
	lf->render = render;
	lf->font = font;
	return font;
}
static render_font* getLuaCacheFont(lua_State *lua,lua_render *lr,const char *name){ // pushes userdata
	render_font *font;
	font_cache *fc;
	
	for(fc = lr->fonts;fc;fc = fc->next)
		if(!strcmp((char*)(fc+1),name)){
			lua_rawgeti(lua,LUA_REGISTRYINDEX,fc->font);
			return ((lua_font*)lua_touserdata(lua,-1))->font;
		}
	font = createLuaFont(lua,lr->render,name);
	fc = malloc(sizeof(font_cache)+strlen(name)+1);
	if(!fc)
		luaL_error(lua,"failed to allocate font cache");
	strcpy((char*)(fc+1),name);
	lua_pushvalue(lua,-1);
	fc->font = luaL_ref(lua,LUA_REGISTRYINDEX);
	fc->next = lr->fonts;
	lr->fonts = fc;
	return font;
}

// TEXT UTILITY
static int getStringFormat(lua_State *lua){ // pushes a value
	lua_pushstring(lua,"string");
	lua_gettable(lua,LUA_GLOBALSINDEX);
	if(!lua_istable(lua,-1))
		return 0;
	lua_pushstring(lua,"format");
	lua_gettable(lua,-2);
	return lua_isfunction(lua,-1);
}
static const char* getLuaString(lua_State *lua){ // converts and shrinks stack to 1 string
	const char *result;
	
	luaL_checkany(lua,1);
	result = lua_tostring(lua,1);
	if(!result){
		lua_pushstring(lua,"tostring"); // tostring is only used if needed
		lua_gettable(lua,LUA_GLOBALSINDEX);
		if(!lua_isfunction(lua,-1))
			luaL_typerror(lua,1,lua_typename(lua,LUA_TSTRING));
		lua_pushvalue(lua,1);
		lua_call(lua,1,1);
		lua_replace(lua,1);
		result = luaL_checkstring(lua,1);
	}
	if(lua_gettop(lua) == 1)
		return result;
	if(getStringFormat(lua)){ // string.format is used if there are multiple args
		lua_insert(lua,1);
		lua_call(lua,lua_gettop(lua)-1,1);
	}
	lua_settop(lua,1);
	return luaL_checkstring(lua,1);
}
static render_font* getLuaTextFont(lua_State *lua,lua_render *lr){ // pushes userdata
	render_font *font;
	
	if(lr->font == LUA_NOREF)
		return getLuaCacheFont(lua,lr,DEFAULT_FONT);
	lua_rawgeti(lua,LUA_REGISTRYINDEX,lr->font);
	return ((lua_font*)lua_touserdata(lua,-1))->font;
}
static void setLuaTextFont(lua_State *lua,lua_render *lr){ // pops userdata
	if(lr->font != LUA_NOREF)
		luaL_unref(lua,LUA_REGISTRYINDEX,lr->font);
	lr->font = luaL_ref(lua,LUA_REGISTRYINDEX);
}
static render_text_draw* getLuaTextArgs(lua_render *lr){
	render_text_draw *args;
	
	args = &lr->textargs;
	if(lr->textsetup)
		return args;
	memset(args,0,sizeof(render_text_draw));
	args->flags = RENDER_TEXT_CENTER | RENDER_TEXT_FIXED_LENGTH | RENDER_TEXT_REDRAW_FOR_RESIZE;
	args->x = 0.5f;
	args->y = 0.195f;
	args->size = DEFAULT_SIZE;
	args->color = 0xFF7F2929;
	lr->textsetup = 1;
	return args;
}
static int styleTextArgs(render_text_draw *args,int style){
	memset(args,0,sizeof(render_text_draw));
	if(style == 1 || style == 3 || style == 5){
		args->flags = RENDER_TEXT_BOLD | RENDER_TEXT_OUTLINE | RENDER_TEXT_FIXED_LENGTH | RENDER_TEXT_REDRAW_FOR_RESIZE;
		if(style == 1)
			args->flags |= RENDER_TEXT_CENTER;
		else if(style == 5)
			args->flags |= RENDER_TEXT_RIGHT;
		args->x = 0.5f;
		args->y = 0.179f;
		args->size = 0.049f;
		args->color = 0xFFC8C878;
	}else if(style == 2 || style == 4 || style == 6){
		args->flags = RENDER_TEXT_BOLD | RENDER_TEXT_SHADOW | RENDER_TEXT_FIXED_LENGTH | RENDER_TEXT_REDRAW_FOR_RESIZE;
		if(style == 2)
			args->flags |= RENDER_TEXT_CENTER;
		else if(style == 6)
			args->flags |= RENDER_TEXT_RIGHT;
		args->x = 0.5f;
		args->y = 0.833f;
		args->size = 0.037f;
		args->color = 0xFFC8C8C8;
	}else
		return 0;
	return 1;
}
static int setLuaTextArgs(lua_render *lr,int style){
	if(!styleTextArgs(&lr->textargs,style))
		return 0;
	lr->textsetup = 1;
	return 1;
}
static void copyLuaTextArgs(lua_render *lr,render_text_draw *copy){
	memcpy(&lr->textargs,copy,sizeof(render_text_draw));
	lr->textsetup = 1;
}
static void resetLuaTextData(lua_State *lua,lua_render *lr){
	if(lr->font != LUA_NOREF){
		luaL_unref(lua,LUA_REGISTRYINDEX,lr->font);
		lr->font = LUA_NOREF;
	}
	lr->textsetup = 0;
}

// TEXT
static int dsl_CreateFont(lua_State *lua){
	luaL_checktype(lua,1,LUA_TSTRING);
	createLuaFont(lua,getLuaRender(lua)->render,lua_tostring(lua,1));
	return 1;
}
static int dsl_DrawText(lua_State *lua){
	render_state *render;
	render_font *font;
	render_text_draw *args;
	lua_render *lr;
	float x,y;
	
	lr = getLuaRender(lua);
	if(lr->cached){
		args = lua_touserdata(lua,3);
		args->text = lua_tostring(lua,1);
		font = ((lua_font*)lua_touserdata(lua,2))->font;
	}else{
		args = getLuaTextArgs(lr);
		args->text = getLuaString(lua); // shrinks stack to 1
		font = getLuaTextFont(lua,lr); // pushes lua_font userdata
	}
	args->length = lua_strlen(lua,1);
	if(!isRenderActive(render = lr->render)){
		args->flags |= RENDER_TEXT_ONLY_MEASURE_TEXT;
		if(!drawRenderString(render,font,args,&x,&y)){
			args->flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
			luaL_error(lua,"%s",getRenderErrorString(render));
		}
		args->flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
		memcpy(lua_newuserdata(lua,sizeof(render_text_draw)),args,sizeof(render_text_draw));
		addCacheCall(lua,lr->cache);
	}else if(!drawRenderString(render,font,args,&x,&y))
		luaL_error(lua,"%s",getRenderErrorString(render));
	if(!lr->cached)
		resetLuaTextData(lua,lr);
	lua_pushnumber(lua,x);
	lua_pushnumber(lua,y);
	return 2;
}
static int dsl_MeasureText(lua_State *lua){
	render_text_draw *args;
	lua_render *lr;
	float x,y;
	
	lr = getLuaRender(lua);
	args = getLuaTextArgs(lr);
	args->text = getLuaString(lua);
	args->length = lua_strlen(lua,1);
	args->flags |= RENDER_TEXT_ONLY_MEASURE_TEXT;
	if(!drawRenderString(lr->render,getLuaTextFont(lua,lr),args,&x,&y)){
		args->flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
		luaL_error(lua,getRenderErrorString(lr->render));
	}
	args->flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
	lua_pushnumber(lua,x);
	lua_pushnumber(lua,y);
	return 2;
}
static int dsl_DiscardText(lua_State *lua){
	resetLuaTextData(lua,getLuaRender(lua));
	return 0;
}
static int dsl_SetTextPosition(lua_State *lua){
	render_text_draw *args;
	
	args = getLuaTextArgs(getLuaRender(lua));
	args->x = luaL_checknumber(lua,1);
	args->y = luaL_checknumber(lua,2);
	return 0;
}
static int dsl_SetTextHeight(lua_State *lua){
	float size;
	
	size = luaL_checknumber(lua,1);
	if(size < 0.0f)
		size = 0.0f;
	#ifdef LIMIT_TEXT_SIZE
	else if(size > LIMIT_TEXT_SIZE)
		size = LIMIT_TEXT_SIZE;
	#endif
	getLuaTextArgs(getLuaRender(lua))->size = size;
	return 0;
}
static int dsl_SetTextScale(lua_State *lua){
	float size;
	
	size = luaL_checknumber(lua,1) * DEFAULT_SIZE;
	if(size < 0.0f)
		size = 0.0f;
	#ifdef LIMIT_TEXT_SIZE
	else if(size > LIMIT_TEXT_SIZE)
		size = LIMIT_TEXT_SIZE;
	#endif
	getLuaTextArgs(getLuaRender(lua))->size = size;
	return 0;
}
static int dsl_SetTextFont(lua_State *lua){
	lua_render *lr;
	lua_font *lf;
	
	lr = getLuaRender(lua);
	lf = lua_touserdata(lua,1);
	if(!lf){
		if(lua_type(lua,1) != LUA_TSTRING)
			luaL_typerror(lua,1,lua_pushfstring(lua,"%s or font",lua_typename(lua,LUA_TSTRING)));
		getLuaCacheFont(lua,lr,lua_tostring(lua,1));
	}
	setLuaTextFont(lua,lr);
	return 0;
}
static int dsl_SetTextAlign(lua_State *lua){
	render_text_draw *args;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	args = getLuaTextArgs(getLuaRender(lua));
	switch(toupper(*lua_tostring(lua,1))){
		case 'L':
			args->flags &= ~(RENDER_TEXT_CENTER | RENDER_TEXT_RIGHT);
			break;
		case 'C':
			args->flags &= ~RENDER_TEXT_RIGHT;
			args->flags |= RENDER_TEXT_CENTER;
			break;
		case 'R':
			args->flags &= ~RENDER_TEXT_CENTER;
			args->flags |= RENDER_TEXT_RIGHT;
			break;
		default:
			lua_pushstring(lua,"unknown horizontal alignment: ");
			lua_pushvalue(lua,1);
			lua_concat(lua,2);
			luaL_argerror(lua,1,lua_tostring(lua,-1));
	}
	if(lua_gettop(lua) < 2)
		return 0;
	luaL_checktype(lua,2,LUA_TSTRING);
	switch(toupper(*lua_tostring(lua,2))){
		case 'T':
			args->flags &= ~(RENDER_TEXT_VERTICALLY_CENTER | RENDER_TEXT_DRAW_TEXT_UPWARDS);
			break;
		case 'C':
			args->flags &= ~RENDER_TEXT_DRAW_TEXT_UPWARDS;
			args->flags |= RENDER_TEXT_VERTICALLY_CENTER;
			break;
		case 'B':
			args->flags &= ~RENDER_TEXT_VERTICALLY_CENTER;
			args->flags |= RENDER_TEXT_DRAW_TEXT_UPWARDS;
			break;
		default:
			lua_pushstring(lua,"unknown vertical alignment: ");
			lua_pushvalue(lua,1);
			lua_concat(lua,2);
			luaL_argerror(lua,1,lua_tostring(lua,-1));
	}
	return 0;
}
static int dsl_SetTextColor(lua_State *lua){
	int arg;
	
	for(arg = 1;arg <= 3;arg++)
		luaL_checktype(lua,arg,LUA_TNUMBER);
	if(lua_gettop(lua) < 4)
		lua_pushnumber(lua,255);
	getLuaTextArgs(getLuaRender(lua))->color = getLuaRgba(lua,1);
	return 0;
}
static int dsl_SetTextShadow(lua_State *lua){
	render_text_draw *args;
	
	args = getLuaTextArgs(getLuaRender(lua));
	args->flags &= ~RENDER_TEXT_OUTLINE;
	args->flags |= RENDER_TEXT_SHADOW;
	args->color2 = lua_gettop(lua) < 3 ? 0 : getLuaRgb(lua,1);
	return 0;
}
static int dsl_SetTextOutline(lua_State *lua){
	render_text_draw *args;
	
	args = getLuaTextArgs(getLuaRender(lua));
	args->flags &= ~RENDER_TEXT_SHADOW;
	args->flags |= RENDER_TEXT_OUTLINE;
	args->color2 = lua_gettop(lua) < 3 ? 0 : getLuaRgb(lua,1);
	return 0;
}
static int dsl_SetTextBold(lua_State *lua){
	if(lua_isboolean(lua,1) && !lua_toboolean(lua,1))
		getLuaTextArgs(getLuaRender(lua))->flags &= ~RENDER_TEXT_BOLD;
	else
		getLuaTextArgs(getLuaRender(lua))->flags |= RENDER_TEXT_BOLD;
	return 0;
}
static int dsl_SetTextBlack(lua_State *lua){
	if(lua_isboolean(lua,1) && !lua_toboolean(lua,1))
		getLuaTextArgs(getLuaRender(lua))->flags &= ~RENDER_TEXT_BLACK;
	else
		getLuaTextArgs(getLuaRender(lua))->flags |= RENDER_TEXT_BLACK;
	return 0;
}
static int dsl_SetTextItalic(lua_State *lua){
	if(lua_isboolean(lua,1) && !lua_toboolean(lua,1))
		getLuaTextArgs(getLuaRender(lua))->flags &= ~RENDER_TEXT_ITALIC;
	else
		getLuaTextArgs(getLuaRender(lua))->flags |= RENDER_TEXT_ITALIC;
	return 0;
}
static int dsl_SetTextClipping(lua_State *lua){
	render_text_draw *args;
	float clip;
	
	args = getLuaTextArgs(getLuaRender(lua));
	if(clip = lua_tonumber(lua,1)){
		if(clip < 0)
			luaL_argerror(lua,1,"negative clipping not allowed");
		args->flags &= ~RENDER_TEXT_WORD_WRAP;
		args->flags |= RENDER_TEXT_CLIP_WIDTH;
		args->width = clip;
	}else
		args->flags &= ~RENDER_TEXT_CLIP_WIDTH;
	if(clip = lua_tonumber(lua,2)){
		if(clip < 0)
			luaL_argerror(lua,1,"negative clipping not allowed");
		args->flags |= RENDER_TEXT_CLIP_HEIGHT;
		args->height = clip;
	}else
		args->flags &= ~RENDER_TEXT_CLIP_HEIGHT;
	return 0;
}
static int dsl_SetTextWrapping(lua_State *lua){
	render_text_draw *args;
	float wrap;
	
	args = getLuaTextArgs(getLuaRender(lua));
	if(wrap = lua_tonumber(lua,1)){
		if(wrap < 0)
			luaL_argerror(lua,1,"negative wrapping not allowed");
		#ifdef LOWER_TEXT_WRAP
		else if(wrap < LOWER_TEXT_WRAP)
			wrap = LOWER_TEXT_WRAP;
		else
		#endif
		if(wrap > 1)
			wrap = 1;
		args->flags &= ~RENDER_TEXT_CLIP_WIDTH;
		args->flags |= RENDER_TEXT_WORD_WRAP;
		args->width = wrap;
	}else
		args->flags &= ~RENDER_TEXT_WORD_WRAP;
	return 0;
}
static int dsl_SetTextRedrawing(lua_State *lua){
	render_text_draw *args;
	
	luaL_checktype(lua,1,LUA_TSTRING);
	args = getLuaTextArgs(getLuaRender(lua));
	switch(toupper(*lua_tostring(lua,1))){
		case 'R':
			args->flags &= ~RENDER_TEXT_FORCE_REDRAW;
			args->flags |= RENDER_TEXT_REDRAW_FOR_RESIZE;
			break;
		case 'A':
			args->flags &= ~RENDER_TEXT_REDRAW_FOR_RESIZE;
			args->flags |= RENDER_TEXT_FORCE_REDRAW;
			break;
		case 'N':
			args->flags &= ~(RENDER_TEXT_FORCE_REDRAW | RENDER_TEXT_REDRAW_FOR_RESIZE);
			break;
		default:
			lua_pushstring(lua,"unknown redrawing mode: ");
			lua_pushvalue(lua,1);
			lua_concat(lua,2);
			luaL_argerror(lua,1,lua_tostring(lua,-1));
	}
	return 0;
}
static int dsl_SetTextResolution(lua_State *lua){
	render_text_draw *args;
	int w,h;
	
	args = getLuaTextArgs(getLuaRender(lua));
	if((w = lua_tonumber(lua,1)) < 0 || w > MAX_CUSTOM_WIDTH)
		luaL_argerror(lua,1,"unsupported width");
	if((h = lua_tonumber(lua,2)) < 0 || h > MAX_CUSTOM_HEIGHT)
		luaL_argerror(lua,2,"unsupported height");
	if(w & h){
		args->flags |= RENDER_TEXT_CUSTOM_WH;
		args->customw = w;
		args->customh = h;
	}else
		args->flags &= ~RENDER_TEXT_CUSTOM_WH;
	return 0;
}
static int dsl_GetTextFormatting(lua_State *lua){
	lua_render *lr;
	
	lua_newtable(lua);
	getLuaTextFont(lua,lr = getLuaRender(lua));
	lua_rawseti(lua,-2,1);
	memcpy(lua_newuserdata(lua,sizeof(render_text_draw)),getLuaTextArgs(lr),sizeof(render_text_draw));
	lua_rawseti(lua,-2,2);
	return 1;
}
static int dsl_PopTextFormatting(lua_State *lua){
	lua_render *lr;
	
	lua_newtable(lua);
	getLuaTextFont(lua,lr = getLuaRender(lua));
	lua_rawseti(lua,-2,1);
	memcpy(lua_newuserdata(lua,sizeof(render_text_draw)),getLuaTextArgs(lr),sizeof(render_text_draw));
	lua_rawseti(lua,-2,2);
	resetLuaTextData(lua,lr);
	return 1;
}
static int dsl_SetTextFormatting(lua_State *lua){
	render_text_draw *copy;
	lua_render *lr;
	int id;
	
	lr = getLuaRender(lua);
	if(id = lua_tonumber(lua,1)){
		if(setLuaTextArgs(lr,id)){
			getLuaCacheFont(lua,lr,"Georgia");
			setLuaTextFont(lua,lr);
			return 0;
		}
		lua_pushstring(lua,"unknown format number: ");
		lua_pushvalue(lua,1);
		lua_concat(lua,2);
		luaL_argerror(lua,1,lua_tostring(lua,-1));
	}
	if(!lua_istable(lua,1))
		luaL_typerror(lua,1,lua_pushfstring(lua,"%s or text format table",lua_typename(lua,LUA_TNUMBER)));
	lua_rawgeti(lua,1,1);
	lua_rawgeti(lua,1,2);
	copy = lua_touserdata(lua,-1);
	if(!copy || !lua_isuserdata(lua,-2))
		luaL_typerror(lua,1,"bad text format table");
	copyLuaTextArgs(lr,copy);
	lua_pop(lua,1);
	setLuaTextFont(lua,lr);
	return 0;
}

// INLINE TYPES
typedef struct inline_state{
	// initial call:
	render_text_draw copy; // original args to copy to real args before draw
	float wrapping; // if non-zero, this is the wrapping width
	char *text; // goes NULL when another inline draw stops this one
	int vararg; // stack position
	unsigned started;
	int duration; // <0 means don't use the inline table
	int align;
	// measure once:
	float linewidths[MAX_INLINE_LINES]; // for manual alignment
	// each draw:
	render_text_draw args;
	render_font *font;
	int measuring; // only measuring this draw
	float basex; // can be changed by ~xy~ or alignment system
	float basey;
	float realbasex; // used as base for alignment, only changed to basex when "repositioned"
	float width; // biggest difference between args and copy x
	float height;
	float lineheight; // biggest drawn height
	int repositioned; // we need to treat ~xy~ like a linebreak
}inline_state;
typedef struct inline_spec{
	const char *name;
	const char *alt;
	void (*func)(lua_State*,lua_render*,inline_state*);
	int args; // if -1, then it is treated as 1 and b is an alternate allowed value
	int a;
	int b;
	int c;
}inline_spec;

// INLINE FUNCTIONS
static void inline_alpha(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = (unsigned)lua_tonumber(lua,is->vararg++) << 24 | is->args.color & 0x00FFFFFF;
}
static void inline_bold(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.flags |= RENDER_TEXT_BOLD;
}
static void inline_blue(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x003C3CC8;
}
static void inline_cyan(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x003CC8C8;
}
static void inline_font(lua_State *lua,lua_render *lr,inline_state *is){
	lua_font *lf;
	
	if(lf = lua_touserdata(lua,is->vararg))
		is->font = lf->font;
	else{
		is->font = getLuaCacheFont(lua,lr,lua_tostring(lua,is->vararg));
		lua_pop(lua,1); // cache fonts never leave the registry so we can just pop it
	}
	is->vararg++;
}
static void inline_green(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x003CC83C;
}
static void inline_height(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.size = lua_tonumber(lua,is->vararg++);
}
static void inline_italic(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.flags |= RENDER_TEXT_ITALIC;
}
static void inline_magenta(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x00C83CC8;
}
static void inline_normal(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.flags &= ~(RENDER_TEXT_BOLD | RENDER_TEXT_ITALIC);
}
static void inline_objective(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x00C8C878;
}
static void inline_red(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x00C83C3C;
}
static void inline_rgb(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | getLuaRgb(lua,is->vararg);
	is->vararg += 3;
}
static void inline_subtitle(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x00C8C8C8;
}
static void inline_scale(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.size = lua_tonumber(lua,is->vararg++) * DEFAULT_SIZE;
}
static void inline_texture(lua_State *lua,lua_render *lr,inline_state *is){
	lua_texture *lt;
	float x,y,w,h;
	
	lt = lua_touserdata(lua,is->vararg++);
	w = (h = is->args.size) * (getRenderTextureAspect(lt->render,lt->texture) / getRenderAspect(lt->render));
	is->args.x += h * 0.25f; // lazy aproximate space needed between text and texture
	x = is->args.x;
	y = is->args.y;
	if(is->args.flags & RENDER_TEXT_DRAW_TEXT_UPWARDS)
		y += h;
	else if(is->args.flags & RENDER_TEXT_VERTICALLY_CENTER)
		y += h * 0.5f;
	if(is->wrapping && x != is->basex && (x + w) - is->basex > is->wrapping){
		x = is->args.x = is->basex;
		y = is->args.y += is->lineheight;
	}
	if(!is->measuring)
		drawRenderTexture(lt->render,lt->texture,x,y,w,h,is->args.color | 0x00FFFFFF);
	is->args.x += w;
}
static void inline_white(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x00FFFFFF;
}
static void inline_xy(lua_State *lua,lua_render *lr,inline_state *is){
	is->basex = lua_tonumber(lua,is->vararg++);
	is->basey = lua_tonumber(lua,is->vararg++);
	is->repositioned = 1;
}
static void inline_yellow(lua_State *lua,lua_render *lr,inline_state *is){
	is->args.color = is->args.color & 0xFF000000 | 0x00C8C83C;
}

// INLINE SPECIFIERS
static const inline_spec inline_specifiers[] = {
	{"a","alpha",&inline_alpha,1,LUA_TNUMBER},
	{"b","bold",&inline_bold,0},
	{"blue",NULL,&inline_blue,0},
	{"c","cyan",&inline_cyan,0},
	{"f","font",&inline_font,-1,LUA_TSTRING,LUA_TUSERDATA},
	{"g","green",&inline_green,0},
	{"height",NULL,&inline_height,1,LUA_TNUMBER},
	{"i","italic",&inline_italic,0},
	{"m","magenta",&inline_magenta,0},
	{"n","normal",&inline_normal,0},
	{"o","objective",&inline_objective,0},
	{"r","red",&inline_red,0},
	{"rgb",NULL,&inline_rgb,3,LUA_TNUMBER,LUA_TNUMBER,LUA_TNUMBER},
	{"s","subtitle",&inline_subtitle,0},
	{"scale",NULL,&inline_scale,1,LUA_TNUMBER},
	{"t","texture",&inline_texture,1,LUA_TUSERDATA},
	{"w","white",&inline_white,0},
	{"xy",NULL,&inline_xy,2,LUA_TNUMBER,LUA_TNUMBER},
	{"y","yellow",&inline_yellow,0}
};

// INLINE UTILITY
static int isInlineSpec(const char *text,const char *name,const char *alt){
	int index;
	
	// should start *after* ~ / +
	for(index = 0;text[index] && name[index];index++)
		if(tolower(text[index]) != name[index])
			return alt ? isInlineSpec(text,alt,NULL) : 0;
	if(text[index] == '~' || text[index] == '+')
		return index + 1; // how many characters were read
	return alt ? isInlineSpec(text,alt,NULL) : 0;
}
static const inline_spec* getInlineSpec(const char *text,int *read){
	const inline_spec *spec;
	int count;
	
	count = sizeof(inline_specifiers) / sizeof(inline_spec);
	while(count){
		spec = inline_specifiers + --count;
		if(*read = isInlineSpec(text,spec->name,spec->alt))
			return spec;
	}
	return NULL;
}
static int isInsideInlineSpec(const char *text){
	int count;
	
	for(count = 0;*text;text++)
		if(*text == '~')
			count++;
	return count % 2;
}

// INLINE PREPROCESSOR
static int copyInlineText(char *dest,const char *src){
	const char *before,*after;
	int length,c,i;
	
	// copies src to dest while moving whitespace behind format specifiers (from "hello ~r~world" to "hello~r~ world")
	length = 0;
	while(c = src[length]){
		if(c == ' ' || c == '\t'){
			before = src + length;
			after = before + 1; // what's after the whitespace?
			while((c = *after) == ' ' || c == '\t')
				after++;
			if(c == '~' && after[1] != '~'){
				dest[length++] = c; // let's copy the format spec first.
				for(i = 1;(c = after[i]) && c != '~';i++)
					dest[length++] = c;
				if(!c)
					return 0; // unfinished format specifier
				dest[length++] = c;
			}
			while(before < after)
				dest[length++] = *(before++); // now copy all the whitespace.
		}else
			dest[length++] = c;
	}
	dest[length] = 0;
	return 1;
}
static inline_state* setupInlineText(lua_State *lua){
	inline_state *is;
	float wrap;
	int stack;
	
	// sets up basic state for inline text drawing
	if(lua_type(lua,1) == LUA_TNUMBER){
		if(lua_type(lua,2) != LUA_TSTRING)
			luaL_typerror(lua,1,lua_typename(lua,LUA_TSTRING)); // no string after width, so it was probably just a number passed by mistake.
		wrap = lua_tonumber(lua,1);
		if(wrap < 0)
			luaL_argerror(lua,1,"width cannot be negative");
		#ifdef LOWER_TEXT_WRAP
		else if(wrap < LOWER_TEXT_WRAP)
			wrap = LOWER_TEXT_WRAP;
		else
		#endif
		if(wrap > 1)
			wrap = 1;
		stack = 2;
	}else{
		luaL_checktype(lua,1,LUA_TSTRING);
		wrap = 0.0f;
		stack = 1;
	}
	is = lua_newuserdata(lua,sizeof(inline_state));
	is->text = lua_newuserdata(lua,lua_strlen(lua,stack) + 1);
	if(!copyInlineText(is->text,lua_tostring(lua,stack)))
		luaL_argerror(lua,stack,"unfinished format specifier");
	lua_replace(lua,stack); // instead of the original string we now just have our char* userdata
	is->wrapping = wrap;
	is->vararg = stack + 1;
	is->started = getGameTimer();
	return is;
}
static int checkInlineArgs(lua_State *lua,inline_state *is,int top,int *repositions){
	const inline_spec *spec;
	const char *text;
	int args,vararg,advance;
	
	// ensures all format specifiers are valid and have valid arguments
	vararg = is->vararg;
	if(repositions)
		*repositions = 0;
	for(text = is->text;*text;text++)
		if(*text == '~'){
			if(text[1] == '~'){
				text++;
				continue;
			}
			do{
				spec = getInlineSpec(text+1,&advance);
				if(!spec)
					luaL_argerror(lua,is->vararg-1,"unknown format specifier");
				if(repositions && !strcmp(spec->name,"xy"))
					*repositions = 1;
				if(args = spec->args){
					if(args != -1){
						// require 1-3 args
						if(top < vararg || lua_type(lua,vararg) != spec->a)
							lua_pop(lua,1),luaL_typerror(lua,vararg,lua_typename(lua,spec->a));
						vararg++;
						if(--args){
							if(top < vararg || lua_type(lua,vararg) != spec->b)
								if(spec->b == LUA_TUSERDATA)
									lua_pop(lua,1),luaL_typerror(lua,vararg,"texture"); // the only spec that has >0 args and userdata b is texture
								else
									lua_pop(lua,1),luaL_typerror(lua,vararg,lua_typename(lua,spec->b));
							vararg++;
							if(--args){
								if((top < vararg || lua_type(lua,vararg) != spec->c))
									lua_pop(lua,1),luaL_typerror(lua,vararg,lua_typename(lua,spec->c));
								vararg++;
							}
						}
					}else{
						// require 1 arg, a or b
						if(top < vararg || ((args = lua_type(lua,vararg)) != spec->a && args != spec->b))
							lua_pop(lua,1),luaL_typerror(lua,vararg,lua_pushfstring(lua,"%s or font",lua_typename(lua,spec->a))); // the only spec that has <0 args is font
						vararg++;
					}
				}
				text += advance;
			}while(*text == '+');
		}
	return vararg; // returns next stack position
}
static void reserveInlineSlot(lua_State *lua,lua_render *lr,inline_state *is,int top,int stack){
	inline_state *old;
	
	lua_rawgeti(lua,LUA_REGISTRYINDEX,lr->inlines);
	if(top < stack + 1)
		lua_pushnumber(lua,1); // default style
	else if(!lua_isnil(lua,stack+1))
		lua_pushvalue(lua,stack+1); // custom style (could be any value)
	else
		luaL_typerror(lua,stack+1,"cannot use nil for style");
	lua_pushvalue(lua,-1);
	lua_rawget(lua,-3);
	if(old = lua_touserdata(lua,-1))
		old->text = NULL; // stop old text
	lua_pop(lua,1);
	lua_pushvalue(lua,top+1);
	lua_rawset(lua,-3); // inlines[style] = inline_state
	lua_pop(lua,1);
}
static void finishInlineArgs(lua_State *lua,lua_render *lr,inline_state *is,int top,int stack){
	render_text_draw *args;
	
	// get the last 2 optional args (duration / style), only leaving a lua_font at stack
	args = &is->copy;
	if(top >= stack){
		// custom duration
		is->duration = luaL_checknumber(lua,stack) * 1000.0f;
		lua_remove(lua,stack); // remove duration
		top--;
	}else
		is->duration = 0;
	if(top < stack){
		// default style
		styleTextArgs(args,1);
		getLuaCacheFont(lua,lr,"Georgia");
	}else if(lua_isnumber(lua,stack)){
		// numbered style
		if(!styleTextArgs(args,lua_tonumber(lua,stack)))
			luaL_argerror(lua,stack+1,lua_pushfstring(lua,"unknown text style: %d",lua_tonumber(lua,stack)));
		getLuaCacheFont(lua,lr,"Georgia");
		lua_remove(lua,stack); // remove number
		top--;
	}else if(lua_istable(lua,stack)){
		// text format table style
		lua_rawgeti(lua,stack,1);
		lua_rawgeti(lua,stack,2);
		if(!lua_isuserdata(lua,-1) || !lua_isuserdata(lua,-2))
			luaL_argerror(lua,stack+1,"bad text format table");
		memcpy(args,lua_touserdata(lua,-1),sizeof(render_text_draw));
		lua_pop(lua,1); // ditch the render_text_draw, we just need lua_font left on stack
		lua_remove(lua,stack); // remove text format table
		top--;
	}else
		luaL_argerror(lua,stack+1,lua_pushfstring(lua,"%s or text format table expected, got %s",lua_typename(lua,LUA_TNUMBER),lua_typename(lua,lua_type(lua,stack))));
	if(top >= stack)
		luaL_argerror(lua,stack+2,"unexpected extra value"); // strictly disallow extra arguments to help debug why a DrawTextInline call may not do what the user wants
	if(args->flags & RENDER_TEXT_RIGHT)
		is->align = 2; // we remember what alignment we want because we need to measure it manually and use left-aligned to actually draw
	else if(args->flags & RENDER_TEXT_CENTER)
		is->align = 1;
	else
		is->align = 0;
	args->flags &= ~(RENDER_TEXT_CENTER | RENDER_TEXT_RIGHT | RENDER_TEXT_WORD_WRAP | RENDER_TEXT_CLIP_WIDTH | RENDER_TEXT_CLIP_HEIGHT);
	args->flags |= RENDER_TEXT_FIXED_LENGTH;
}

// INLINE DRAWING
static int getInlineChunkSize(lua_State *lua,lua_render *lr,inline_state *is,const char *text,int *wrapped){
	render_text_draw *args;
	float wrap,width,w;
	int c,count;
	int space; // if we're in whitespace
	int endline; // last char before space
	int lastline; // last endline each time we hit space
	
	if(wrap = is->wrapping){
		args = &is->args;
		width = args->x - is->basex;
		lastline = endline = space = 0;
	}
	for(count = 0;(c = text[count]) && c != '~' && c != '\n';count++)
		if(wrap){
			if(!isspace(c)){
				endline = count;
				space = 0;
			}else if(!space){
				if(lastline){
					args->text = text;
					args->length = count;
					args->flags |= RENDER_TEXT_ONLY_MEASURE_TEXT;
					if(!drawRenderString(lr->render,is->font,args,&w,NULL))
						luaL_error(lua,getRenderErrorString(lr->render));
					args->flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
					if(width + w > wrap){
						*wrapped = 1;
						return lastline + 1;
					}
				}
				lastline = endline;
				space = 1;
			}
		}
	if(wrap && !space && lastline){
		args->text = text;
		args->length = count;
		args->flags |= RENDER_TEXT_ONLY_MEASURE_TEXT;
		if(!drawRenderString(lr->render,is->font,args,&w,NULL))
			luaL_error(lua,getRenderErrorString(lr->render));
		args->flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
		if(width + w > wrap){
			*wrapped = 1;
			return lastline + 1;
		}
	}
	*wrapped = 0;
	return count;
}
static void drawInlineChunk(lua_State *lua,lua_render *lr,inline_state *is,const char *text,int chunk){
	render_text_draw *args;
	float w,h;
	
	args = &is->args;
	args->text = text;
	args->length = chunk;
	if(is->measuring)
		args->flags |= RENDER_TEXT_ONLY_MEASURE_TEXT;
	if(!drawRenderString(lr->render,is->font,args,&w,&h))
		luaL_error(lua,getRenderErrorString(lr->render));
	args->x += w;
	if(is->lineheight < h)
		is->lineheight = h;
	if(is->width < (w = args->x - is->basex))
		is->width = w;
	if(is->height < (h = (args->y + h) - is->basey))
		is->height = h;
}
static const char* drawInlineLine(lua_State *lua,lua_render *lr,inline_state *is,const char *text,float *width){
	render_text_draw *args;
	int chunk,wrapped;
	float ybefore;
	
	args = &is->args;
	if(*text == '+' && isInsideInlineSpec(text))
		goto specifiers;
	while(*text == ' ' || *text == '\t')
		text++;
	while(*text)
		if(*text == '~' && text[1] != '~'){
			specifiers:
			*width = args->x - is->basex; // in case we reposition we want to know what the width was
			ybefore = args->y;
			do{
				(*getInlineSpec(text+1,&chunk)->func)(lua,lr,is);
				text += chunk;
				if(is->repositioned){
					is->repositioned = 0; // reposition should only happen at the start of inline text so we're not too worried about proper line widths if there's a texture before a reposition
					is->realbasex = is->basex;
					args->x = is->basex;
					args->y = is->basey;
					if(*text != '+')
						text++;
					return text; // ~xy~ needs to be treated like the end of a line so alignment can be re-measured
				}
			}while(*text == '+');
			text++; // just need to go past the final ~ now
			if(ybefore != args->y)
				return text; // means texture must have done a linebreak for wrapping
		}else{
			chunk = getInlineChunkSize(lua,lr,is,text,&wrapped);
			if(text[chunk] == '~' && text[chunk+1] == '~'){ // ~~ means draw 1 but read over 2
				drawInlineChunk(lua,lr,is,text,chunk+1);
				chunk += 2;
			}else if(chunk)
				drawInlineChunk(lua,lr,is,text,chunk);
			text += chunk;
			if(wrapped || *text == '\n'){
				do{
					if(*text == '\n')
						text++;
					args->y += is->lineheight;
				}while(*text == '\n');
				*width = args->x - is->basex;
				args->x = is->basex;
				return text;
			}
		}
	*width = args->x - is->basex;
	return NULL;
}
static void drawInlineText(lua_State *lua,lua_render *lr,inline_state *is,int measure,float *w,float *h){
	render_text_draw *args;
	const char *text;
	float width;
	int vararg;
	int line;
	
	vararg = is->vararg; // saved so it can be reset after
	args = &is->args;
	memcpy(args,&is->copy,sizeof(render_text_draw)); // copy base args to real args
	is->font = ((lua_font*)lua_touserdata(lua,-1))->font;
	is->measuring = measure;
	is->basex = is->copy.x;
	is->basey = is->copy.y;
	is->realbasex = is->basex;
	is->width = 0.0f;
	is->height = 0.0f;
	is->lineheight = 0.0f;
	is->repositioned = 0;
	line = 0;
	for(text = is->text;text;text = drawInlineLine(lua,lr,is,text,&width))
		if(is->align){
			if(measure){
				if(line)
					is->linewidths[line-1] = width;
			}else if(width = is->linewidths[line]){
				if(is->align == 1)
					is->basex = is->realbasex - width * 0.5f;
				else
					is->basex = is->realbasex - width;
				args->x = is->basex;
			}
			if(++line == MAX_INLINE_LINES)
				luaL_error(lua,"tried to draw too many lines");
		}
	if(measure && line)
		is->linewidths[line-1] = width;
	is->vararg = vararg;
	*w = is->width;
	*h = is->height;
}

// INLINE
static int dsl_DrawTextInline(lua_State *lua){
	lua_render *lr;
	inline_state *is;
	int top,stack,measure,repositions;
	float w,h;
	
	measure = 0;
	lr = getLuaRender(lua);
	if(lr->cached){
		is = lua_touserdata(lua,-2);
		if(!is->text) // another inline call must have NULL'd our text
			return 0;
		if(is->duration > 0 && getGameTimer() >= is->started + (unsigned)is->duration) // if duration is zero we'll still draw but just not cache it
			return 0;
	}else{
		top = lua_gettop(lua); // track the top of user args since we're about to push inline_state userdata
		is = setupInlineText(lua);
		stack = checkInlineArgs(lua,is,top,&repositions);
		if(!repositions && (top < stack || luaL_checknumber(lua,stack) >= 0))
			reserveInlineSlot(lua,lr,is,top,stack); // only reserve if not respositioning and duration != -1
		finishInlineArgs(lua,lr,is,top,stack);
		if(!isRenderActive(lr->render))
			measure = 1;
		else if(is->align)
			drawInlineText(lua,lr,is,1,&w,&h); // still measure before the real draw if using special align
	}
	// stack: [width], text, [...], inline_state, lua_font
	drawInlineText(lua,lr,is,measure || (lr->cached && getGamePaused()),&w,&h);
	if(measure || is->duration > 0)
		addCacheCall(lua,lr->cache); // add cache if it was just a measure this time || it wasn't just a single frame draw
	lua_pushnumber(lua,w);
	lua_pushnumber(lua,h);
	return 2;
}
static int dsl_MeasureTextInline(lua_State *lua){
	lua_render *lr;
	inline_state *is;
	int top;
	float w,h;
	
	lr = getLuaRender(lua);
	top = lua_gettop(lua);
	is = setupInlineText(lua);
	finishInlineArgs(lua,lr,is,top,checkInlineArgs(lua,is,top,NULL));
	drawInlineText(lua,lr,is,1,&w,&h);
	lua_pushnumber(lua,w);
	lua_pushnumber(lua,h);
	return 2;
}

// ADVANCED
static int dsl_ClearDisplay(lua_State *lua){
	render_state *render;
	
	if(!clearRenderDisplay(render = getLuaRender(lua)->render))
		luaL_error(lua,"%s",getRenderErrorString(render));
	return 0;
}
static int dsl_CreateRenderTarget(lua_State *lua){
	luaL_checktype(lua,1,LUA_TNUMBER);
	luaL_checktype(lua,2,LUA_TNUMBER);
	createLuaTexture(lua,getDslState(lua,1),getLuaRender(lua)->render,NULL,lua_tonumber(lua,1),lua_tonumber(lua,2));
	return 1;
}
static int dsl_DrawBackBufferOntoTarget(lua_State *lua){
	lua_texture *lt;
	
	lt = lua_touserdata(lua,1);
	if(!lt)
		luaL_typerror(lua,1,"texture");
	if(!fillRenderTextureBackBuffer(lt->render,lt->texture))
		luaL_error(lua,getRenderErrorString(lt->render));
	return 0;
}
static int dsl_DrawGameWorldImmediately(lua_State *lua){
	(*(void(__fastcall*)(void*,void*,void*))0x4F1800)((void*)0xC3CC68,NULL,*(void**)0xC1AE7C); // update camera something?
	(*(void(__fastcall*)(void*))0x4F3900)((void*)0xC3CC68); // update camera manager
	(*(void(__cdecl*)())0x4096F0)(); // set camera?
	(*(void(__cdecl*)())0x55E2C0)();
	/*
	(*(void(__fastcall*)(void*,void*,void*))0x4F1800)((void*)0xC3CC68,NULL,*(void**)0xC1AE7C); // update camera something?
	(*(void(__fastcall*)(void*))0x4F3900)((void*)0xC3CC68); // update camera manager
	(*(void(__cdecl*)())0x43B580)(); // set camera?
	(*(void(__cdecl*)())0x43AE60)(); // clear?
	(*(void(__cdecl*)())0X404090)();
	(*(void(__cdecl*)())0X404120)();
	(*(void(__cdecl*)())0x43AEB0)();
	(*(void(__cdecl*)())0x55E2C0)(); // draw world
	*/
	return 0;
}
static int dsl_SetRendererAlphaBlending(lua_State *lua){
	luaL_checktype(lua,1,LUA_TBOOLEAN);
	if(IDirect3DDevice9_SetRenderState(getRenderDevice(getLuaRender(lua)->render),D3DRS_ALPHABLENDENABLE,lua_toboolean(lua,1)) != D3D_OK)
		luaL_error(lua,"failed to set renderer state");
	return 0;
}

// DEBUG
static int dsl_DrawDebugMemory(lua_State *lua){
	render_text_draw args;
	render_state *render;
	render_font *font;
	char buffer[16];
	unsigned char *address;
	int bytes,b;
	int offset;
	float w,h;
	script **s;
	
	if((s = lua_touserdata(lua,1)) && *s){
		address = (*s)->script_object;
		bytes = SCRIPT_BYTES;
		offset = luaL_checknumber(lua,2);
	}else{
		address = (void*)strtoul(luaL_checkstring(lua,1),NULL,0);
		bytes = strtol(luaL_checkstring(lua,2),NULL,0);
		offset = luaL_checknumber(lua,3);
	}
	render = getRenderOrCache(lua,getLuaRender(lua));
	if(!render)
		return 0;
	if(offset){
		address += offset;
		bytes -= offset;
	}
	memset(&args,0,sizeof(render_text_draw));
	args.text = buffer;
	args.color = 0xFFFFFFFF;
	args.size = 0.01f;
	args.flags = RENDER_TEXT_ONLY_MEASURE_TEXT;
	font = getLuaCacheFont(lua,getLuaRender(lua),"Lucida Console");
	buffer[0] = '0';
	buffer[1] = '0';
	buffer[2] = 0;
	if(!drawRenderString(render,font,&args,&w,&h))
		luaL_error(lua,"%s",getRenderErrorString(render));
	w += 0.004f;
	for(b = 0;b < 16;b++)
		if(!drawRenderRectangle(render,w*(b%16),0.0f,w,1.0f,b%2?0xFF102010:0xFF001000))
			luaL_error(lua,"%s",getRenderErrorString(render));
	args.flags &= ~RENDER_TEXT_ONLY_MEASURE_TEXT;
	for(b = 0;b < bytes;b++){
		args.x = (b % 16) * w + 0.002f;
		args.y = (b / 16) * h;
		args.color = address[b] ? 0xFF40FF40 : 0xFF408040;
		sprintf(buffer,"%.2X",(int)address[b]);
		if(!drawRenderString(render,font,&args,NULL,NULL))
			luaL_error(lua,"%s",getRenderErrorString(render));
	}
	for(b = 0;b <= ((bytes - 1) / 16);b++){
		args.x = w * 16 + 0.002f;
		args.y = b * h;
		args.color = b % 2 ? 0xFF409640 : 0xFF40FF40;
		sprintf(buffer,"+0x%X",(unsigned)b*16+offset);
		if(!drawRenderString(render,font,&args,NULL,NULL))
			luaL_error(lua,"%s",getRenderErrorString(render));
	}
	return 0;
}
static int dsl_GetTextCacheDebug(lua_State *lua){
	int created;
	
	lua_pushnumber(lua,getRenderStringCacheCount(getLuaRender(lua)->render,&created));
	lua_pushnumber(lua,created);
	return 2;
}

// MAIN
static int updatingThread(lua_State *lua,lua_render *lr,thread *t){
	lr->cache = lr->caches;
	resetLuaTextData(lua,lr);
	return 0;
}
static int updatedManager(lua_State *lua,lua_render *lr,int type){
	if(type == PRE_FADE_THREAD)
		runCacheCalls(lua,lr,lr->caches+CACHE_PRE_FADE);
	else if(type == POST_WORLD_THREAD)
		runCacheCalls(lua,lr,lr->caches+CACHE_POST_WORLD);
	else if(type == DRAWING_THREAD)
		runCacheCalls(lua,lr,lr->caches+CACHE_POST_FADE);
	return 0;
}
int dslopen_render(lua_State *lua){
	dsl_state *dsl;
	lua_render *lr;
	
	dsl = getDslState(lua,1);
	lr = setupLuaRender(lua,dsl);
	addScriptEventCallback(dsl->events,EVENT_THREAD_UPDATE,&updatingThread,lr);
	addScriptEventCallback(dsl->events,EVENT_MANAGER_UPDATE,(script_event_cb)&updatedManager,lr);
	// DISPLAY
	lua_register(lua,"GetDisplayResolution",&dsl_GetDisplayResolution);
	lua_register(lua,"GetDisplayAspectRatio",&dsl_GetDisplayAspectRatio);
	// LAYER
	lua_register(lua,"SetDrawLayer",&dsl_SetDrawLayer);
	// BASIC
	lua_register(lua,"DrawRectangle",&dsl_DrawRectangle);
	// TEXTURE
	lua_register(lua,"CreateTexture",&dsl_CreateTexture);
	lua_register(lua,"DrawTexture",&dsl_DrawTexture);
	lua_register(lua,"DrawTexture2",&dsl_DrawTexture2);
	lua_register(lua,"SetTextureBounds",&dsl_SetTextureBounds);
	lua_register(lua,"GetTextureResolution",&dsl_GetTextureResolution);
	lua_register(lua,"GetTextureAspectRatio",&dsl_GetTextureAspectRatio);
	lua_register(lua,"GetTextureDisplayAspectRatio",&dsl_GetTextureDisplayAspectRatio);
	// TEXT
	lua_register(lua,"CreateFont",&dsl_CreateFont);
	lua_register(lua,"DrawText",&dsl_DrawText);
	lua_register(lua,"MeasureText",&dsl_MeasureText);
	lua_register(lua,"DiscardText",&dsl_DiscardText);
	lua_register(lua,"SetTextPosition",&dsl_SetTextPosition);
	lua_register(lua,"SetTextHeight",&dsl_SetTextHeight);
	lua_register(lua,"SetTextScale",&dsl_SetTextScale);
	lua_register(lua,"SetTextFont",&dsl_SetTextFont);
	lua_register(lua,"SetTextAlign",&dsl_SetTextAlign);
	lua_register(lua,"SetTextColor",&dsl_SetTextColor);
	lua_register(lua,"SetTextColour",&dsl_SetTextColor);
	lua_register(lua,"SetTextShadow",&dsl_SetTextShadow);
	lua_register(lua,"SetTextOutline",&dsl_SetTextOutline);
	lua_register(lua,"SetTextBold",&dsl_SetTextBold);
	lua_register(lua,"SetTextBlack",&dsl_SetTextBlack);
	lua_register(lua,"SetTextItalic",&dsl_SetTextItalic);
	lua_register(lua,"SetTextClipping",&dsl_SetTextClipping);
	lua_register(lua,"SetTextWrapping",&dsl_SetTextWrapping);
	lua_register(lua,"SetTextRedrawing",&dsl_SetTextRedrawing);
	//lua_register(lua,"SetTextResolution",&dsl_SetTextResolution);
	lua_register(lua,"GetTextFormatting",&dsl_GetTextFormatting);
	lua_register(lua,"PopTextFormatting",&dsl_PopTextFormatting);
	lua_register(lua,"SetTextFormatting",&dsl_SetTextFormatting);
	// INLINE
	lua_register(lua,"DrawTextInline",&dsl_DrawTextInline);
	lua_register(lua,"MeasureTextInline",&dsl_MeasureTextInline);
	// ADVANCED
	lua_register(lua,"ClearDisplay",&dsl_ClearDisplay);
	lua_register(lua,"CreateRenderTarget",&dsl_CreateRenderTarget);
	lua_register(lua,"DrawBackBufferOntoTarget",&dsl_DrawBackBufferOntoTarget);
	//lua_register(lua,"DrawGameWorldImmediately",&dsl_DrawGameWorldImmediately);
	lua_register(lua,"SetRendererAlphaBlending",&dsl_SetRendererAlphaBlending);
	// DEBUG
	if(dsl->flags & DSL_ADD_DEBUG_FUNCS){
		lua_register(lua,"DrawDebugMemory",&dsl_DrawDebugMemory);
		lua_register(lua,"GetTextCacheDebug",&dsl_GetTextCacheDebug);
	}
	return 0;
}