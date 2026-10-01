/* DERPY'S SCRIPT LOADER: UTILITY
	
	a few general purpose DSL functions
	
*/

#ifndef DSL_UTILITY_H
#define DSL_UTILITY_H

struct dsl_state;

#ifndef DSL_SERVER_VERSION
#include "client/render.h"
#endif

// GET STATE
dsl_state* getDslState(lua_State *lua,int unprotected); // if unprotected is 0, NULL can be returned instead of a lua error

// GET PREFIX
const char* getDslPrintPrefix(dsl_state *dsl,int include_thread); // returns a static string that should be copied if needed long

// FILE SAFETY
int isDslFileSafe(struct dsl_state *dsl,const unsigned char *path); // returns non-zero if path is a file and is allowed to be accessed

// FILE UTILITY
int checkDslPathExists(const char *path,int skiplast); // creates all needed directories in the path and returns non-zero if nothing fails

// GENERAL EVENTS
#ifndef DSL_SERVER_VERSION
void runDslWindowStyleEvent(dsl_state *dsl,HWND window,int show);
#endif

// LUA TEXTURES
#ifndef DSL_SERVER_VERSION
typedef struct lua_texture{
	render_state *render;
	render_texture *texture;
	float x1;
	float y1;
	float x2;
	float y2;
}lua_texture;
void createLuaTexture(lua_State *lua,dsl_state *dsl,render_state *render,const char *path,int width,int height); // pushes userdata
void createLuaTexture2(lua_State *lua,render_state *render,const char *name,const char *path); // pushes userdata, no file safety!
#endif

// STRING UTILITY
int dslisenum(const char *a,const char *b); // b should be all uppercase
int dslstrcmp(const char *a,const char *b); // returns a - b on the first difference
int dslstrncmp(const char *a,const char *b,int n); // strcmp but will only read n characters

#endif