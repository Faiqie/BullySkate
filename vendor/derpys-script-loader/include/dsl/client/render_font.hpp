/* DERPY'S SCRIPT LOADER: FONT RENDERER
	
	this is a component of the renderer for drawing fonts using dwrite and d3d9
	all the text drawing here just works as a helper for render.cpp
	
*/

typedef void font_renderer; // type doesn't matter, just needs to be a pointer.
typedef void font_renderer_font;
extern "C" {
	font_renderer* createFontRenderer(IDirect3DDevice9 *device,int *failcode);
	void destroyFontRenderer(font_renderer *fr);
	font_renderer_font* createFontRendererFont(font_renderer *fr,void *file,const char *name,int *failcode); // file is currently unsupported
	void destroyFontRendererFont(font_renderer *fr,font_renderer_font *font);
	void startFontRendererCycle(font_renderer *fr,int width,int height);
	void finishFontRendererCycle(font_renderer *fr);
	IDirect3DTexture9* drawFontRendererTexture(font_renderer *fr,font_renderer_font *font,render_text_draw *args,float *width,float *height,int *failcode);
	unsigned getFontRendererCacheCount(font_renderer *fr,int *created); // just debug function
}