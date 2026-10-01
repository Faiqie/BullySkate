/* DERPY'S SCRIPT LOADER: FONT RENDERER
	
	this is a component of the renderer for drawing fonts using dwrite and d3d9
	all the text drawing here just works as a helper for render.cpp
	
*/

#define RENDER_LIMITED_H
#include <dsl/dsl.h>
#include <string.h>
#include <wchar.h>
#include <dwrite.h>

#define RENDER_FONT_API extern "C"
//#define MAX_CACHE_COUNT 1024 // optional limitation
//#define DEBUG_BITMAP_CHANGES

/* HOW IT WORKS
	
	DirectWrite functions are only invoked when new text is drawn, even though this is designed to be used per-frame.
	This is done using a cache system that holds onto d3d9 textures and only re-draws when needed.
	First, a font_renderer is created using createFontRenderer(). This object is used for every other function too.
	Then, a font_renderer_font is created with createFontRendererFont(). This is used for the drawing function.
	Before drawing, a font render cycle must be started with startFontRendererCycle().
	Starting the cycle sets the renderer's cache_ptr to the start of its linked cache_list.
	When drawing, it checks if cache_ptr already has the texture needed for the draw.
	If it does not, then the entire cache_list is checked instead.
	If the right cache is found, then that cache is removed from the list and inserted where cache_ptr is.
	If a cache was found then it is used for the draw and cache_ptr moves forward one.
	Otherwise, a new cache is created and inserted at cache_ptr and cache_ptr still moves forward.
	If cache_ptr is NULL, then it means there are no more cache entries and anything new adds to the end.
	Then finally, the cycle is finished with finishFontRendererCycle(). This deletes cache_ptr (if not NULL) and everything past.
	
	Basically, that means any text drawn in a render cycle is cached. Any existing cache not drawn in a render cycle is released.
	So only text that is being drawn every cycle (frame) is kept in the cache.
	All the cache is kept in order in the linked list so that, if no text is changing, the whole cycle is done really quickly.
	
	How does a draw happen? Well, the text format is already created with the font_renderer_font. So we'll start there.
	If the format's size is different from the draw's size, then the format object will be re-created.
	The format's word wrap mode will be set as needed, then we are ready to move onto the layout.
	The layout and gdi bitmap render target are created. Then the text is drawn onto that gdi bitmap using pure white.
	Then the affected rect of the bitmap is used to create a d3d9 texture. All bitmap content in that rect is copied to the texture.
	The bitmap is pure white (0x00FFFFFF) so any of the RGB values can be used as alpha (<< 24), and we will only have an alpha.
	Then white is just put back onto the value (| 0x00FFFFFF) and we have a white transparent d3d9 texture that can be used and colored seperately.
	That texture is put in the cache along with all the other cache attributes.
	
	ASCII? Yes. It is ascii only. I can't really be bothered with the rest right now. Maybe I'll add more support later, but probably not.
	Lua is ascii anyway so it wouldn't make sense to bother.
	
	The bitmap is now only created when the size of the font renderer (the d3d viewport) changes instead of every draw like previously stated.
	
*/

// TYPES.
struct font_renderer_font{
	IDWriteFontCollection *collection;
	IDWriteTextFormat *format;
	wchar_t *name;
	float size; // if cache is redrawn with a different size than this, then the format is re-created.
	int wrap; // just so SetWordWrapping isn't called every frame.
	DWRITE_TEXT_ALIGNMENT align;
	font_renderer_font *next; // just so all fonts can be kept in the renderer and be cleaned up when destroyed.
};
struct font_renderer_cache{
	font_renderer_font *font;
	unsigned length;
	char *text;
	float size;
	DWRITE_FONT_WEIGHT bold;
	int italic;
	int wordwrap;
	DWRITE_TEXT_ALIGNMENT align; // only matters when wordwrap is set
	float wrapwidth; // only matters when wordwrap is set
	float width;
	float height;
	int usepoints; // use points for scaling
	int usedpiscale; // allow dpi scaling
	int customw; // use a custom canvas resolution (zero if not)
	int customh;
	IDirect3DTexture9 *texture;
	font_renderer_cache *next;
};
class font_renderer : public IDWriteTextRenderer{
	public:
	int width;
	int height;
	IDirect3DDevice9 *device;
	IDWriteFactory *factory;
	IDWriteRenderingParams *params;
	IDWriteGdiInterop *gdi;
	IDWriteBitmapRenderTarget *bitmap; // now only created once the first time it is needed (before it was just temporary)
	int bitmapw; // bitmap only resizes when reasonable
	int bitmaph;
	font_renderer_font *fonts;
	font_renderer_cache *cache_list; // linked list of all cache objects
	font_renderer_cache *cache_ptr; // "current" cache, determines what cache gets scrapped after the font render cycle, and helps optimize speed a little.
	int cache_created;
	HRESULT WINAPI GetCurrentTransform(void*,DWRITE_MATRIX*);
	HRESULT WINAPI GetPixelsPerDip(void*,FLOAT*);
	HRESULT WINAPI IsPixelSnappingDisabled(void*,BOOL*);
	HRESULT WINAPI DrawGlyphRun(void*,FLOAT,FLOAT,DWRITE_MEASURING_MODE,const DWRITE_GLYPH_RUN*,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*);
	HRESULT WINAPI DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*);
	HRESULT WINAPI DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*);
	HRESULT WINAPI DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*);
	HRESULT WINAPI QueryInterface(const IID &,void**);
	ULONG WINAPI AddRef(void);
	ULONG WINAPI Release(void);
};

// DWRITE RENDERER.
HRESULT WINAPI font_renderer::GetCurrentTransform(void *ptr,DWRITE_MATRIX *matrix){
	memset(matrix,0,sizeof(DWRITE_MATRIX));
	matrix->m11 = 1.0f;
	matrix->m22 = 1.0f;
	return S_OK;
}
HRESULT WINAPI font_renderer::GetPixelsPerDip(void *ptr,FLOAT *result){
	*result = 1.0f;
	return S_OK;
}
HRESULT WINAPI font_renderer::IsPixelSnappingDisabled(void *ptr,BOOL *result){
	*result = 1;
	return S_OK;
}
HRESULT WINAPI font_renderer::DrawGlyphRun(void *ptr,FLOAT x,FLOAT y,DWRITE_MEASURING_MODE measure,const DWRITE_GLYPH_RUN *run,const DWRITE_GLYPH_RUN_DESCRIPTION *description,IUnknown *effect){
	return bitmap->DrawGlyphRun(x,y,measure,run,params,0xFFFFFFFF,NULL);
}
HRESULT WINAPI font_renderer::DrawInlineObject(void *ptr,FLOAT x,FLOAT y,IDWriteInlineObject *inlobj,BOOL arg1,BOOL arg2,IUnknown *effect){
	return E_NOTIMPL;
}
HRESULT WINAPI font_renderer::DrawStrikethrough(void *ptr,FLOAT x,FLOAT y,const DWRITE_STRIKETHROUGH *strike,IUnknown *effect){
	return E_NOTIMPL;
}
HRESULT WINAPI font_renderer::DrawUnderline(void *ptr,FLOAT x,FLOAT y,const DWRITE_UNDERLINE *underline,IUnknown *effect){
	return E_NOTIMPL;
}
HRESULT WINAPI font_renderer::QueryInterface(const IID &iid,void **arg){
	return E_NOTIMPL;
}
ULONG WINAPI font_renderer::AddRef(){
	return 0;
}
ULONG WINAPI font_renderer::Release(){
	return 0;
}

// MISC UTILITY.
#ifdef DEBUG_BITMAP_CHANGES
static void logBitmapChanges(int init,int width,int height){
	FILE *file;
	
	if(file = fopen("bitmap.txt","ab")){
		if(init)
			fprintf(file,"create bitmap: %d x %d\n",width,height);
		else
			fprintf(file,"resize bitmap: %d x %d\n",width,height);
		fclose(file);
	}
}
#endif
static BOOL WINAPI procMonitorEnum(HMONITOR monitor,HDC hdc,LPRECT rect,LPARAM arg){
	*(HMONITOR*)arg = monitor;
	return 0;
}
static wchar_t* makeWideCharFromAscii(const char *str){
	wchar_t *ptr;
	size_t count;
	
	count = strlen(str) + 1;
	if(ptr = (wchar_t*) malloc(sizeof(wchar_t)*count)){
		swprintf(ptr,count,L"%hs",str);
		return ptr;
	}
	return NULL;
}

// TEXT DRAWING.
static IDWriteTextFormat* getFontFormat(font_renderer *fr,font_renderer_font *font,float size,int wrap,DWRITE_TEXT_ALIGNMENT align){
	IDWriteTextFormat *format;
	
	// The font format is only re-created if it didn't exist or the size changed.
	// Then needed settings are applied if they weren't already.
	format = font->format;
	if(!format || font->size != size){
		if(format)
			format->Release();
		if(fr->factory->CreateTextFormat(font->name,font->collection,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-US",&format) != S_OK)
			return NULL;
		font->size = size;
		format->SetWordWrapping((font->wrap = wrap) ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
		format->SetTextAlignment(font->align = align);
		return font->format = format;
	}
	if(font->wrap != wrap)
		format->SetWordWrapping((font->wrap = wrap) ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
	if(font->align != align)
		format->SetTextAlignment(font->align = align);
	return format;
}
static IDWriteTextLayout* createLayout(font_renderer *fr,font_renderer_cache *cache,IDWriteTextFormat *format,const wchar_t *text,unsigned length,int width,int height){
	IDWriteTextLayout *layout;
	struct DWRITE_TEXT_RANGE range;
	
	if(fr->factory->CreateTextLayout(text,length,format,width,height,&layout) != S_OK)
		return NULL;
	range.startPosition = 0;
	range.length = cache->length;
	if(cache->bold)
		layout->SetFontWeight(cache->bold,range); // could fail but why care? it's probably just not supported anyways which isn't worth raising an error.
	if(cache->italic)
		layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC,range);
	return layout;
}
static void fillTextureUsingBitmapData(uint32_t *tex,uint32_t *bit,int x,int y,int tw,int th,int bw,int bh){
	int i,remain;
	
	remain = bw - tw;
	if(remain < 0)
		return; // shouldn't happen
	bit += x + y * bw;
	for(;th;th--){
		for(i = tw;i;i--)
			*(tex++) = *(bit++) << 24 | 0x00FFFFFF;
		bit += remain;
	}
}
static IDirect3DTexture9* createTextureFromBitmap(font_renderer *fr,int x,int y,int w,int h,int *width,int *height,int *failcode){
	IDirect3DTexture9 *texture;
	D3DLOCKED_RECT rect;
	DIBSECTION dib;
	HBITMAP object;
	HDC hdc;
	
	if(w <= 0 || h <= 0)
		return *failcode = RENDER_FAIL_TEXT_NOT_BIG_ENOUGH, NULL;
	hdc = fr->bitmap->GetMemoryDC();
	object = (HBITMAP) GetCurrentObject(hdc,OBJ_BITMAP);
	if(!object || !GetObject(object,sizeof(DIBSECTION),&dib))
		return *failcode = RENDER_FAIL_GDI, NULL;
	if(fr->device->CreateTexture(w,h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,NULL) != D3D_OK)
		return *failcode = RENDER_FAIL_CREATE_TEXTURE, NULL;
	if(texture->LockRect(0,&rect,NULL,0) != D3D_OK){
		texture->Release();
		return *failcode = RENDER_FAIL_D3D9, NULL;
	}
	fillTextureUsingBitmapData((uint32_t*)rect.pBits,(uint32_t*)dib.dsBm.bmBits,x,y,w,h,*width,*height);
	texture->UnlockRect(0);
	*width = w;
	*height = h;
	return texture;
}
static int clearBitmap(font_renderer *fr,size_t bw,size_t bh,int *failcode){
	DIBSECTION dib;
	HBITMAP object;
	HDC hdc;
	
	hdc = fr->bitmap->GetMemoryDC();
	object = (HBITMAP) GetCurrentObject(hdc,OBJ_BITMAP);
	if(!object || !GetObject(object,sizeof(DIBSECTION),&dib))
		return *failcode = RENDER_FAIL_GDI, 1;
	memset(dib.dsBm.bmBits,0,bw*bh*sizeof(uint32_t));
	return 0;
}
static int drawCacheTexture(font_renderer *fr,font_renderer_cache *cache,const wchar_t *text,int *failcode){
	IDWriteGdiInterop *gdi;
	IDWriteTextFormat *format;
	IDWriteTextLayout *layout;
	IDWriteBitmapRenderTarget *target;
	DWRITE_TEXT_METRICS metrics;
	float ratio,scale;
	int width,height;
	int tx,ty,tw,th;
	int w,h;
	
	if(fr->width <= 0 || fr->height <= 0)
		return *failcode = RENDER_FAIL_TEXT_NOT_BIG_ENOUGH;
	gdi = fr->gdi;
	if(!gdi){
		if(fr->factory->GetGdiInterop(&gdi) != S_OK)
			return *failcode = RENDER_FAIL_DW_GET_GDI_INTEROP, 0;
		fr->gdi = gdi;
	}
	target = fr->bitmap;
	if(!target){
		#ifdef DEBUG_BITMAP_CHANGES
		logBitmapChanges(1,fr->width,fr->height);
		#endif
		if(gdi->CreateBitmapRenderTarget(NULL,fr->width,fr->height,&target) != S_OK)
			return *failcode = RENDER_FAIL_DW_CREATE_BITMAP, 0;
		fr->bitmap = target;
		fr->bitmapw = fr->width;
		fr->bitmaph = fr->height;
	}else if(fr->width != fr->bitmapw || fr->height != fr->bitmaph){
		#ifdef DEBUG_BITMAP_CHANGES
		logBitmapChanges(0,fr->width,fr->height);
		#endif
		if(target->Resize(fr->width,fr->height) != S_OK)
			return *failcode = RENDER_FAIL_DWRITE, 0;
		fr->bitmapw = fr->width;
		fr->bitmaph = fr->height;
	}
	width = cache->customw ? cache->customw : fr->width;
	if(width > fr->width)
		width = fr->width;
	w = width;
	if(cache->wordwrap && (w *= cache->wrapwidth) > width)
		w = width;
	height = cache->customh ? cache->customh : fr->height;
	h = height;
	if(height > fr->height)
		height = fr->height;
	if(w <= 0 || height <= 0)
		return *failcode = RENDER_FAIL_TEXT_NOT_BIG_ENOUGH;
	ratio = 1.0f / target->GetPixelsPerDip();
	if(cache->usepoints)
		scale = (1.0f / 72.0f) * 96.0f * (cache->usedpiscale ? 1.0f : ratio);
	else
		scale = cache->usedpiscale ? h : h * ratio;
	format = getFontFormat(fr,cache->font,cache->size*scale,cache->wordwrap,cache->align);
	if(!format)
		return *failcode = RENDER_FAIL_DW_TEXT_FORMAT, 0;
	layout = createLayout(fr,cache,format,text,cache->length,w*ratio,h*ratio);
	if(!layout)
		return *failcode = RENDER_FAIL_DW_TEXT_LAYOUT, 0;
	if(clearBitmap(fr,fr->width,fr->height,failcode)){
		layout->Release();
		return 0;
	}
	if(layout->Draw(NULL,fr,0.0f,0.0f) != S_OK || layout->GetMetrics(&metrics) != S_OK){
		layout->Release();
		return *failcode = RENDER_FAIL_DW_DRAW, 0;
	}
	layout->Release();
	tx = metrics.left / ratio;
	ty = metrics.top / ratio;
	tw = metrics.width / ratio;
	th = metrics.height / ratio;
	if(tx < 0)
		tx = 0;
	if(ty < 0)
		ty = 0;
	if(tx + tw > w)
		tw = w - tx;
	if(ty + th > h)
		th = h - ty;
	w = fr->width;
	h = fr->height;
	cache->texture = createTextureFromBitmap(fr,tx,ty,tw,th,&w,&h,failcode);
	if(!cache->texture)
		return 0;
	cache->width = (float) w / width;
	cache->height = (float) h / height;
	return 1;
}

// TEXT CACHE.
static unsigned getCacheCount(font_renderer *fr){
	font_renderer_cache *cache;
	unsigned count;
	
	count = 0;
	for(cache = fr->cache_list;cache;cache = cache->next)
		count++;
	return count;
}
static int fillCacheArgument(font_renderer_cache *c,font_renderer_font *font,render_text_draw *args){
	unsigned long flags;
	char ch;
	
	flags = args->flags;
	if(!font)
		return 0;
	c->font = font;
	c->text = (char*) args->text; // it's not gonna be modified so we just strip the qualifier so it doesn't upset the struct
	if(!c->text)
		return 0;
	c->size = args->size;
	if(!c->size)
		return 0;
	c->length = flags & RENDER_TEXT_FIXED_LENGTH ? args->length : strlen(c->text);
	if(flags & RENDER_TEXT_BLACK)
		c->bold = DWRITE_FONT_WEIGHT_BLACK;
	else if(flags & RENDER_TEXT_BOLD)
		c->bold = DWRITE_FONT_WEIGHT_BOLD;
	else
		c->bold = (DWRITE_FONT_WEIGHT) 0;
	c->italic = flags & RENDER_TEXT_ITALIC;
	if(c->wordwrap = flags & RENDER_TEXT_WORD_WRAP)
		c->wrapwidth = args->width;
	if(flags & RENDER_TEXT_CENTER)
		c->align = DWRITE_TEXT_ALIGNMENT_CENTER;
	else if(flags & RENDER_TEXT_RIGHT)
		c->align = DWRITE_TEXT_ALIGNMENT_TRAILING;
	else
		c->align = DWRITE_TEXT_ALIGNMENT_LEADING;
	c->usepoints = flags & RENDER_TEXT_USE_POINT_SIZE;
	c->usedpiscale = flags & RENDER_TEXT_USE_DPI_SCALING;
	if(flags & RENDER_TEXT_CUSTOM_WH){
		c->customw = args->customw;
		c->customh = args->customh;
	}else
		c->customw = c->customh = 0;
	return 1;
}
static int doesCacheMatch(font_renderer_cache *cache,font_renderer_cache *arg,unsigned long flags){
	// Check if this cache matches what we want to draw.
	// If no cache matches, a new cache is created and drawn.
	if(flags & RENDER_TEXT_FORCE_REDRAW)
		return 0;
	if((flags & RENDER_TEXT_REDRAW_FOR_RESIZE) && (cache->size != arg->size || cache->customw != arg->customw || cache->customh != arg->customh))
		return 0;
	if(cache->bold != arg->bold || cache->italic != arg->italic)
		return 0;
	if(cache->font != arg->font || cache->length != arg->length || cache->wordwrap != arg->wordwrap || cache->align != arg->align || cache->usepoints != arg->usepoints || cache->usedpiscale != arg->usedpiscale)
		return 0;
	if(cache->wordwrap && (cache->wrapwidth != arg->wrapwidth || cache->size != arg->size))
		return 0;
	if(strncmp(cache->text,arg->text,cache->length))
		return 0;
	return 1;
}
static void insertCache(font_renderer *fr,font_renderer_cache *before,font_renderer_cache *cache){
	font_renderer_cache *iter,*next;
	
	iter = fr->cache_list;
	if(iter == before){ // insert at start
		cache->next = iter;
		fr->cache_list = cache;
		return;
	}
	for(;next = iter->next;iter = next){ // or insert into chain
		if(next == before){
			cache->next = next;
			iter->next = cache;
			return;
		}
	}
	cache->next = NULL;
	iter->next = cache; // or at the end
}
static font_renderer_cache* removeCache(font_renderer *fr,font_renderer_cache *cache){
	font_renderer_cache *iter,*next;
	
	iter = fr->cache_list;
	if(iter == cache){ // remove from start
		fr->cache_list = cache->next;
		return cache;
	}
	for(;next = iter->next;iter = next){ // or remove from the chain
		if(next == cache){
			iter->next = cache->next;
			return cache;
		}
	}
	return NULL;
}
static font_renderer_cache* createCache(font_renderer *fr,font_renderer_cache *copy,int *failcode){
	font_renderer_cache *cache;
	wchar_t *wtext;
	
	#ifdef MAX_CACHE_COUNT
	if(getCacheCount(fr) >= MAX_CACHE_COUNT)
		return *failcode = RENDER_FAIL_TEXT_LIMIT_REACHED, NULL;
	#endif
	cache = (font_renderer_cache*) malloc(sizeof(font_renderer_cache));
	if(!cache)
		return *failcode = RENDER_FAIL_MEMORY, NULL;
	cache->length = copy->length;
	cache->text = (char*) malloc(cache->length);
	if(!cache->text){
		free(cache);
		return *failcode = RENDER_FAIL_MEMORY, NULL;
	}
	memcpy(cache->text,copy->text,cache->length);
	wtext = makeWideCharFromAscii(cache->text);
	if(!wtext){
		free(cache->text);
		free(cache);
		return *failcode = RENDER_FAIL_MEMORY, NULL;
	}
	cache->size = copy->size;
	cache->font = copy->font;
	cache->bold = copy->bold;
	cache->italic = copy->italic;
	cache->wordwrap = copy->wordwrap;
	cache->align = copy->align;
	cache->wrapwidth = copy->wrapwidth;
	cache->usepoints = copy->usepoints;
	cache->usedpiscale = copy->usedpiscale;
	cache->customw = copy->customw;
	cache->customh = copy->customh;
	if(drawCacheTexture(fr,cache,wtext,failcode)){
		fr->cache_created++;
		free(wtext);
		return cache;
	}
	free(cache->text);
	free(cache);
	free(wtext);
	return NULL;
}
static void destroyCache(font_renderer *fr,font_renderer_cache *cache){ // doesn't remove
	fr->cache_created--;
	cache->texture->Release();
	free(cache->text);
	free(cache);
}

// MAIN FUNCTIONS.
RENDER_FONT_API void startFontRendererCycle(font_renderer *fr,int width,int height){
	// cache_ptr walks through the list during the render cycle.
	// if any cache is drawn out of place, it is moved to cache_ptr.
	// any new cache is inserted at cache_ptr.
	if(width != fr->width || height != fr->height){ // Reset cache on resolution change.
		font_renderer_cache *cache,*next;
		for(next = fr->cache_list;cache = next;){
			next = cache->next;
			destroyCache(fr,cache);
		}
		fr->cache_list = NULL;
	}
	fr->width = width;
	fr->height = height;
	fr->cache_ptr = fr->cache_list;
}
RENDER_FONT_API void finishFontRendererCycle(font_renderer *fr){
	font_renderer_cache *cache,*next;
	
	// any cache at cache_ptr and beyond wasn't drawn this cycle.
	// so all that cache is destroyed, because we only keep cache drawn every cycle.
	next = fr->cache_ptr;
	while(cache = next){
		next = cache->next;
		destroyCache(fr,removeCache(fr,cache));
	}
}
RENDER_FONT_API IDirect3DTexture9* drawFontRendererTexture(font_renderer *fr,font_renderer_font *font,render_text_draw *args,float *width,float *height,int *failcode){
	font_renderer_cache argument,*cache;
	
	if(!fillCacheArgument(&argument,font,args))
		return *failcode = RENDER_FAIL_BAD_CALL_ARGUMENTS, NULL;
	if((cache = fr->cache_ptr) && doesCacheMatch(cache,&argument,args->flags)){ // Firstly, is it the current ordered cache?
		fr->cache_ptr = cache->next;
		if(cache->size == argument.size){
			*width = cache->width;
			*height = cache->height;
		}else{
			*width = cache->width * (argument.size / cache->size);
			*height = cache->height * (argument.size / cache->size);
		}
		return cache->texture;
	}
	for(cache = fr->cache_list;cache;cache = cache->next){ // If not, is there any cache at all?
		if(doesCacheMatch(cache,&argument,args->flags)){
			insertCache(fr,fr->cache_ptr,removeCache(fr,cache));
			if(cache->size == argument.size){
				*width = cache->width;
				*height = cache->height;
			}else{
				*width = cache->width * (argument.size / cache->size);
				*height = cache->height * (argument.size / cache->size);
			}
			return cache->texture;
		}
	}
	if(cache = createCache(fr,&argument,failcode)){ // Or finally, create the new needed cache.
		insertCache(fr,fr->cache_ptr,cache);
		*width = cache->width;
		*height = cache->height;
		return cache->texture;
	}
	return NULL;
}
RENDER_FONT_API font_renderer_font* createFontRendererFont(font_renderer *fr,void *file,const char *name,int *failcode){
	font_renderer_font *font;
	wchar_t *wname;
	
	wname = makeWideCharFromAscii(name);
	if(!wname)
		return *failcode = RENDER_FAIL_MEMORY, NULL;
	font = (font_renderer_font*) malloc(sizeof(font_renderer_font));
	if(!font){
		free(wname);
		return *failcode = RENDER_FAIL_MEMORY, NULL;
	}
	font->collection = NULL; // TODO: implement this
	font->format = NULL; // will be initialized on draw, along with size and wrap.
	font->name = wname;
	font->next = fr->fonts;
	fr->fonts = font;
	return font;
}
RENDER_FONT_API void destroyFontRendererFont(font_renderer *fr,font_renderer_font *font){
	font_renderer_font *remove,*next;
	
	remove = fr->fonts;
	if(remove == font)
		fr->fonts = font->next;
	else
		for(;next = remove->next;remove = next){
			if(next == font){
				remove->next = font->next;
				break;
			}
		}
	if(font->collection)
		font->collection->Release();
	if(font->format)
		font->format->Release();
	free(font->name);
	free(font);
}
RENDER_FONT_API font_renderer* createFontRenderer(IDirect3DDevice9 *device,int *failcode){
	HMONITOR monitor;
	font_renderer *fr;
	IDWriteFactory *factory;
	IDWriteRenderingParams *params;
	
	// switched from DWRITE_FACTORY_TYPE_SHARED to DWRITE_FACTORY_TYPE_ISOLATED to see if it helps avoid the memory leak
	if(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,__uuidof(IDWriteFactory),(IUnknown**)&factory) != S_OK){
		if(failcode)
			*failcode = RENDER_FAIL_DW_FACTORY;
		return NULL;
	}
	monitor = NULL;
	EnumDisplayMonitors(NULL,NULL,&procMonitorEnum,(LPARAM)&monitor);
	if(!monitor || factory->CreateMonitorRenderingParams(monitor,&params) != S_OK){
		if(failcode)
			*failcode = RENDER_FAIL_DW_MONITOR;
		factory->Release();
		return NULL;
	}
	fr = new font_renderer;
	if(!fr){
		if(failcode)
			*failcode = RENDER_FAIL_MEMORY;
		params->Release();
		factory->Release();
		return NULL;
	}
	fr->width = 0;
	fr->height = 0;
	fr->device = device;
	fr->factory = factory;
	fr->params = params;
	fr->gdi = NULL;
	fr->bitmap = NULL;
	fr->bitmapw = 0;
	fr->bitmaph = 0;
	fr->fonts = NULL;
	fr->cache_list = NULL;
	fr->cache_ptr = NULL;
	fr->cache_created = 0;
	return fr;
}
RENDER_FONT_API void destroyFontRenderer(font_renderer *fr){
	font_renderer_cache *cache,*nextcache;
	font_renderer_font *font,*nextfont;
	
	for(nextcache = fr->cache_list;cache = nextcache;){
		nextcache = cache->next;
		destroyCache(fr,cache);
	}
	for(nextfont = fr->fonts;font = nextfont;){
		nextfont = font->next;
		destroyFontRendererFont(fr,font);
	}
	if(fr->bitmap)
		fr->bitmap->Release();
	fr->params->Release();
	fr->factory->Release();
	delete fr;
}
RENDER_FONT_API unsigned getFontRendererCacheCount(font_renderer *fr,int *created){
	*created = fr->cache_created;
	return getCacheCount(fr);
}