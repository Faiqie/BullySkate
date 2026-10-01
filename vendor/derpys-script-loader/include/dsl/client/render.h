/* DERPY'S SCRIPT LOADER: RENDERER
	
	ABOUT:
	this renderer exists for one very specific purpose.
	and that is to draw simple shapes, textures, and fonts on an existing d3d9 device.
	
	RENDER STATUS:
	functions that can fail will return zero (or NULL) to signal failure, and non-zero for success.
	at any time, getRenderError() can be used to get the status of the last failable render call.
	for printing, getRenderErrorString() returns a string describing what the status means.
	
	RENDER SETUP:
	first call createRender() to create a new render state and get a pointer back to it.
	when you are done using the render, destroyRender() will clean everything associated with it.
	destroyRender() should only be called after a successful createRender().
	if createRender() fails it is because of memory allocation or d3d9 and the error functions can't work.
	
	RENDER CYCLE:
	start rendering by calling startRender(), if non-zero is returned you can now call drawing functions.
	when you're done (but only if started successfully) you should call finishRender().
	these functions scale the rest of the drawing functions, backup the old render state, set it to what it needs to be, and restores it.
	all drawing functions take normalized coordinates (from 0 to 1).
	isRenderActive() returns if a render cycle is currently active.
	getRenderAspect() returns the current aspect ratio of the render cycle (width / height).
	getRenderResolution() returns the current width and height of the render cycle.
	
	DRAWING FUNCTIONS:
	drawRenderRectangle() can be used to draw a simple rectangle.
	drawRenderTextureUV() draws a created texture.
	drawRenderTexture() draws a texture without the need for UV coords.
	drawRenderString() draws text using directwrite.
	
	TEXTURE DRAWING:
	some drawing functions take textures that you first need to create with createRenderTexture().
	any texture functions that take FILE* as an argument will leave the file at an undefined position.
	when you are done with the texture, you can use destroyRenderTexture() to free resources.
	all created textures are destroyed when the renderer is destroyed.
	getRenderTextureAspect() and getRenderTextureResolution() can be used to query a texture's size.
	
	STRING DRAWING:
	createRenderFont() can be used to create a font, which can be destroyed with destroyRenderFont().
	if it is not manually destroyed, it will still be destroyed along with the renderer.
	these fonts are used with the drawRenderString() function.
	getRenderStringCacheCount() can be used for debugging the font renderer's cache.
	
*/

#ifndef DSL_RENDER_H
#define DSL_RENDER_H

struct dsl_file;

// STATUS CODES (int).
#define RENDER_OK 0
#define RENDER_FAIL_UNK 1
#define RENDER_FAIL_GDI 2
#define RENDER_FAIL_D3D9 3
#define RENDER_FAIL_DWRITE 4
#define RENDER_FAIL_MEMORY 5
#define RENDER_FAIL_READ_FILE 6
#define RENDER_FAIL_MISSING_FILE 7
#define RENDER_FAIL_INVALID_FILE 8
#define RENDER_FAIL_UNSUPPORTED_FILE 9
#define RENDER_FAIL_NOT_READY_FOR_DRAW 10
#define RENDER_FAIL_BAD_CALL_ARGUMENTS 11
#define RENDER_FAIL_TEXT_LIMIT_REACHED 12
#define RENDER_FAIL_TEXT_NOT_BIG_ENOUGH 13
#define RENDER_FAIL_DW_GET_GDI_INTEROP 14
#define RENDER_FAIL_DW_CREATE_BITMAP 15
#define RENDER_FAIL_DW_TEXT_FORMAT 16
#define RENDER_FAIL_DW_TEXT_LAYOUT 17
#define RENDER_FAIL_DW_FACTORY 18
#define RENDER_FAIL_DW_MONITOR 19
#define RENDER_FAIL_DW_DRAW 20
#define RENDER_FAIL_DIF_FORMATS 21
#define RENDER_FAIL_CREATE_TEXTURE 22
#define RENDER_FAIL_VIEWPORT_ASSERT 23

// TEXT FLAGS.
#define RENDER_TEXT_CENTER 1 // only choose this or right
#define RENDER_TEXT_RIGHT 2
#define RENDER_TEXT_SHADOW 4
#define RENDER_TEXT_OUTLINE 8
#define RENDER_TEXT_WORD_WRAP 16 // use the width argument for word wrap, only choose this or clip_width
#define RENDER_TEXT_FIXED_LENGTH 32
#define RENDER_TEXT_USE_POINT_SIZE 64 // interpret the size argument as point size instead of normalized screen height
#define RENDER_TEXT_USE_DPI_SCALING 128 // allow the size of the font to change for the system dpi
#define RENDER_TEXT_CLIP_WIDTH 256 // use the width argument to clip text
#define RENDER_TEXT_CLIP_HEIGHT 512 // use the height argument to clip text
#define RENDER_TEXT_FORCE_REDRAW 1024 // force dwrite to update the texture
#define RENDER_TEXT_REDRAW_FOR_RESIZE 2048 // tell dwrite to re-draw if the size changed
#define RENDER_TEXT_DRAW_TEXT_UPWARDS 4096 // interpret the y argument as the bottom of the text, only use this or vertically_center
#define RENDER_TEXT_VERTICALLY_CENTER 8192 // interpret the y argument as the middle of the text
#define RENDER_TEXT_ONLY_MEASURE_TEXT 16384 // don't draw the text, only render it in order to get the size
#define RENDER_TEXT_BOLD 32768
#define RENDER_TEXT_ITALIC 65536
#define RENDER_TEXT_BLACK 131072 // black font weight
#define RENDER_TEXT_CUSTOM_WH 262144 // use customw and customh

// RENDER TYPES.
typedef struct render_state render_state;
typedef struct render_texture render_texture;
typedef struct render_font render_font;
typedef struct render_text_draw{ // used as an argument for drawRenderString (only text/font/color need to be non-zero)
	unsigned long flags;
	const char *text;
	unsigned length; // requires flag RENDER_TEXT_FIXED_LENGTH
	float x;
	float y;
	float size;
	float width; // requires flag RENDER_TEXT_WORD_WRAP or RENDER_TEXT_CLIP_WIDTH
	float height; // requiers flag RENDER_TEXT_CLIP_HEIGHT
	DWORD color; // 0xAARRGGBB
	DWORD color2; // for shadow / outline
	int customw; // for custom canvas size when drawing, requires RENDER_TEXT_CUSTOM_WH
	int customh;
}render_text_draw;

// RENDER FUNCTIONS START.
#ifndef RENDER_LIMITED_H
#ifdef __cplusplus
extern "C" {
#endif

// D3D9 DEVICE.
#define getRenderDevice(render) (*(IDirect3DDevice9**)render)

// ERROR HANDLING.
int getRenderError(render_state *render);
const char* getRenderErrorString(render_state *render);

// RENDER STATE.
render_state* createRender(IDirect3DDevice9 *device); // returns NULL on failure.
void destroyRender(render_state *render);

// RENDER CYCLE.
int startRender(render_state *render);
void finishRender(render_state *render);
int startRenderDwriteCycle(render_state *render);
void finishRenderDwriteCycle(render_state *render);
int isRenderActive(render_state *render);
float getRenderAspect(render_state *render);
void getRenderResolution(render_state *render,int *width,int *height);

// DRAWING.
#define drawRenderTexture(render,texture,x,y,w,h,argb) drawRenderTextureUV(render,texture,x,y,w,h,argb,0.0f,0.0f,1.0f,1.0f)
int drawRenderRectangle(render_state *render,float x,float y,float w,float h,DWORD argb);
int drawRenderTextureUV(render_state *render,render_texture *texture,float x,float y,float w,float h,DWORD argb,float x1,float y1,float x2,float y2);
int drawRenderTextureUV2(render_state *render,render_texture *texture,float x,float y,float w,float h,DWORD argb,float x1,float y1,float x2,float y2,float heading); // centered and rotated
int drawRenderString(render_state *render,render_font *font,render_text_draw *args,float *advance_x,float *advance_y);
int clearRenderDisplay(render_state *render);

// TEXTURES.
#define createRenderTexture(render,source) createRenderTextureEx(render,source,NULL)
#define createRenderTexture2(render,file) createRenderTextureEx(render,NULL,file)
render_texture* createRenderTextureEx(render_state *render,FILE *source,struct dsl_file *file); // returns NULL on failure, takes a png file opened in binary as an argument.
void destroyRenderTexture(render_state *render,render_texture *texture);
float getRenderTextureAspect(render_state *render,render_texture *texture);
void getRenderTextureResolution(render_state *render,render_texture *texture,int *width,int *height);

// TARGETS.
render_texture* createRenderTextureTarget(render_state *render,int width,int height);
int fillRenderTextureBackBuffer(render_state *render,render_texture *texture);

// FONTS.
render_font* createRenderFont(render_state *render,void *file,const char *name); // returns NULL on failure, file can be NULL for system.
void destroyRenderFont(render_state *render,render_font *font);
unsigned getRenderStringCacheCount(render_state *render,int *created);

// RENDER FUNCTIONS END.
#ifdef __cplusplus
}
#endif
#endif

#endif