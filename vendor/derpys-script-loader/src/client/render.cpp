/* DERPY'S SCRIPT LOADER: RENDERER
	
	this renderer is meant to draw basic shapes, textures, and fonts on a per-frame basis
	see render.h for usage
	
*/

#define RENDER_LIMITED_H
#include <dsl/dsl.h>
#include <dsl/client/render_font.hpp>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <png.h>

#define RENDER_API extern "C"
#define PNG_SIG_BYTES 8
#define SHADOW_SIZE 0.1f
#define OUTLINE_SIZE 0.067f
#define OUTLINE_ALPHA_MULT 0.5f // shadow / outline alpha multiplier
#define FALLBACK_ERROR_STRING "really severe error"
#define FVF_FLAGS D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1
#define RENDER_SUCCEED(render) (render->status = RENDER_OK, 1)
#define RENDER_FAIL(render,code) (render->status = code, 0)

// OPTIONAL.
#define ASSERT_VIEWPOINT
#define USE_STATE_BLOCKS
//#define SUPPRESS_TEXT_LIMIT_ERROR // succeed text draws even if the text limit error happened

// CONSTANTS.
static const int32_t g_quadindices[] = {0,1,2,0,2,3}; // a quad using D3DPT_TRIANGLELIST

// TYPES.
struct render_texture{
	IDirect3DTexture9 *texture;
	unsigned width;
	unsigned height;
	render_texture *next;
};
struct render_vertex{
	float x;
	float y;
	float z;
	float w;
	DWORD argb;
	float u;
	float v;
};
struct render_state{
	// D3D9 OBJECTS.
	IDirect3DDevice9 *device;
	#ifdef USE_STATE_BLOCKS
	IDirect3DStateBlock9 *backup;
	#else
	DWORD backup[11]; // all state and FVF
	IDirect3DVertexShader9 *vbackup;
	IDirect3DPixelShader9 *pbackup;
	#endif
	render_texture *first_texture;
	
	// RENDERER STATE.
	int width;
	int height;
	int active;
	int status;
	char *cbuffer; // used by some functions that return strings
	render_vertex vertices[4]; // enough for polygons and quads, the 2 primitives the renderer cares about
	font_renderer *fontrender;
};

// PNG LOADER.
static int setupTextureReader(png_struct **ppng,png_info **pinfo){
	png_struct *png;
	png_info *info;
	
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if(!png)
		return 0;
	info = png_create_info_struct(png);
	if(!info){
		png_destroy_read_struct(&png,NULL,NULL);
		return 0;
	}
	*ppng = png;
	*pinfo = info;
	return 1;
}
static int checkTextureFile(png_struct *png,FILE *file){
	png_byte sig[PNG_SIG_BYTES];
	
	if(setjmp(png_jmpbuf(png)))
		return 0;
	if(!fread((char*)sig,PNG_SIG_BYTES,1,file) || png_sig_cmp(sig,0,PNG_SIG_BYTES))
		return 0;
	png_init_io(png,file);
	png_set_sig_bytes(png,PNG_SIG_BYTES);
	return 1;
}
static void readTextureFile(png_struct *png,char *buffer,size_t bytes){
	if(readDslFile((dsl_file*)png_get_io_ptr(png),buffer,bytes) != bytes)
		png_error(png,"corrupted png file");
}
static int checkTextureFile2(png_struct *png,dsl_file *file){
	png_byte sig[PNG_SIG_BYTES];
	
	if(setjmp(png_jmpbuf(png)))
		return 0;
	if(readDslFile(file,(char*)sig,PNG_SIG_BYTES) != PNG_SIG_BYTES || png_sig_cmp(sig,0,PNG_SIG_BYTES))
		return 0;
	png_set_read_fn(png,file,(png_rw_ptr)&readTextureFile);
	png_set_sig_bytes(png,PNG_SIG_BYTES);
	return 1;
}
static D3DFORMAT readTextureInfo(png_struct *png,png_info *info,png_uint_32 *width,png_uint_32 *height){
	int bit_depth;
	int color_type;
	int interlace_method;
	int compression_method;
	int filter_method;
	
	if(setjmp(png_jmpbuf(png)))
		return D3DFMT_UNKNOWN;
	png_read_info(png,info);
	png_get_IHDR(png,info,width,height,&bit_depth,&color_type,&interlace_method,&compression_method,&filter_method);
	if(png_get_valid(png,info,PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if(bit_depth < 8)
		png_set_packing(png);
	else if(bit_depth == 16)
		png_set_strip_16(png);
	if(color_type == PNG_COLOR_TYPE_GRAY){
		png_set_gray_to_rgb(png);
		png_set_add_alpha(png,0xFF,PNG_FILLER_AFTER);
		return D3DFMT_A8R8G8B8;
	}else if(color_type == PNG_COLOR_TYPE_GRAY_ALPHA){
		png_set_gray_to_rgb(png); // alpha is stripped, unsure how to fix
		return D3DFMT_A8R8G8B8;
	}else if(color_type == PNG_COLOR_TYPE_PALETTE){
		png_set_palette_to_rgb(png);
		png_set_bgr(png);
		png_set_add_alpha(png,0xFF,PNG_FILLER_AFTER);
		return D3DFMT_A8R8G8B8;
	}else if(color_type == PNG_COLOR_TYPE_RGB){
		png_set_add_alpha(png,0xFF,PNG_FILLER_AFTER);
		png_set_bgr(png);
		return D3DFMT_A8R8G8B8;
	}else if(color_type == PNG_COLOR_TYPE_RGB_ALPHA){
		png_set_bgr(png);
		return D3DFMT_A8R8G8B8;
	}
	return D3DFMT_UNKNOWN;
}
static int readTextureImage(png_struct *png,png_info *info,png_uint_32 height,png_byte *bits){
	png_uint_32 row;
	png_byte **rows;
	size_t bytes;
	
	rows = NULL;
	if(setjmp(png_jmpbuf(png))){
		if(rows)
			free(rows);
		return 0;
	}
	png_set_interlace_handling(png);
	png_read_update_info(png,info); // update transformations so rowbytes is accurate
	bytes = png_get_rowbytes(png,info);
	rows = (png_byte**) malloc(sizeof(png_byte*)*height);
	if(!rows)
		return 0;
	for(row = 0;row < height;row++)
		rows[row] = bits + bytes*row;
	png_read_image(png,rows);
	png_read_end(png,info);
	free(rows);
	return 1;
}
static int createTextureFromFile(IDirect3DDevice9 *device,FILE *file,dsl_file *dfile,unsigned *rwidth,unsigned *rheight,IDirect3DTexture9 **rtexture){
	png_struct *png;
	png_info *info;
	D3DFORMAT format;
	png_uint_32 width,height;
	IDirect3DTexture9 *texture;
	D3DLOCKED_RECT rect;
	
	// Start reading texture file.
	if(!setupTextureReader(&png,&info))
		return RENDER_FAIL_UNK;
	if(file ? !checkTextureFile(png,file) : !checkTextureFile2(png,dfile)){
		png_destroy_read_struct(&png,&info,NULL);
		return RENDER_FAIL_INVALID_FILE;
	}
	
	// Read image info to transform and init D3D texture.
	format = readTextureInfo(png,info,&width,&height);
	if(format == D3DFMT_UNKNOWN){
		png_destroy_read_struct(&png,&info,NULL);
		return RENDER_FAIL_UNSUPPORTED_FILE;
	}
	if(device->CreateTexture(width,height,1,0,format,D3DPOOL_MANAGED,&texture,NULL) != D3D_OK){
		png_destroy_read_struct(&png,&info,NULL);
		return RENDER_FAIL_CREATE_TEXTURE;
	}
	
	// Actually fill the D3D texture up.
	if(texture->LockRect(0,&rect,NULL,0) != D3D_OK){
		texture->Release();
		png_destroy_read_struct(&png,&info,NULL);
		return RENDER_FAIL_D3D9;
	}
	if(!readTextureImage(png,info,height,(png_byte*)rect.pBits)){
		texture->Release();
		png_destroy_read_struct(&png,&info,NULL);
		return RENDER_FAIL_READ_FILE;
	}
	texture->UnlockRect(0);
	
	// Cleanup and return the created texture.
	png_destroy_read_struct(&png,&info,NULL);
	*rwidth = width;
	*rheight = height;
	*rtexture = texture;
	return RENDER_OK;
}

// RENDER UTILITY.
#ifdef ASSERT_VIEWPOINT
static int assertViewport(int w,int h){
	if(w <= 0 || h <= 0)
		return 1;
	return 0;
}
#endif

// RENDER TEXTURES.
static void freeRenderTexture(render_texture *texture){
	texture->texture->Release();
	free(texture);
}
static void destroyAllRenderTextures(render_state *render){
	render_texture *rt,*next;
	
	next = render->first_texture;
	while(rt = next){
		next = rt->next;
		freeRenderTexture(rt);
	}
	render->first_texture = NULL;
}
RENDER_API render_texture* createRenderTextureEx(render_state *render,FILE *source,dsl_file *file){
	render_texture *texture;
	
	texture = (render_texture*) malloc(sizeof(render_texture));
	if(!texture)
		return RENDER_FAIL(render,RENDER_FAIL_MEMORY);
	if(render->status = createTextureFromFile(render->device,source,file,&texture->width,&texture->height,&texture->texture)){
		free(texture);
		return NULL;
	}
	texture->next = render->first_texture;
	render->first_texture = texture;
	render->status = RENDER_OK;
	return texture;
}
RENDER_API void destroyRenderTexture(render_state *render,render_texture *texture){
	render_texture *rt;
	
	rt = render->first_texture;
	if(rt == texture){
		render->first_texture = texture->next;
	}else{
		while(rt->next){
			if(rt->next == texture){
				rt->next = texture->next;
				break;
			}
			rt = rt->next;
		}
	}
	freeRenderTexture(texture);
}
RENDER_API float getRenderTextureAspect(render_state *render,render_texture *texture){
	return (float) texture->width / texture->height;
}
RENDER_API void getRenderTextureResolution(render_state *render,render_texture *texture,int *width,int *height){
	*width = texture->width;
	*height = texture->height;
}

// RENDER TARGETS.
RENDER_API render_texture* createRenderTextureTarget(render_state *render,int width,int height){
	render_texture *texture;
	
	texture = (render_texture*) malloc(sizeof(render_texture));
	if(!texture)
		return RENDER_FAIL(render,RENDER_FAIL_MEMORY);
	if(render->device->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture->texture,NULL) != D3D_OK){
		free(texture);
		return RENDER_FAIL(render,RENDER_FAIL_CREATE_TEXTURE);
	}
	texture->width = width;
	texture->height = height;
	texture->next = render->first_texture;
	render->first_texture = texture;
	render->status = RENDER_OK;
	return texture;
}
RENDER_API int fillRenderTextureBackBuffer(render_state *render,render_texture *texture){
	IDirect3DSurface9 *surface,*copy;
	D3DSURFACE_DESC desc,desc2;
	
	if(render->device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&surface) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	if(surface->GetDesc(&desc) != D3D_OK || texture->texture->GetSurfaceLevel(0,&copy) != D3D_OK){
		surface->Release();
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	}
	if(copy->GetDesc(&desc2) != D3D_OK || desc.Format != desc2.Format){
		copy->Release();
		surface->Release();
		return RENDER_FAIL(render,RENDER_FAIL_DIF_FORMATS);
	}
	if(render->device->StretchRect(surface,NULL,copy,NULL,D3DTEXF_NONE) != D3D_OK){
		copy->Release();
		surface->Release();
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	}
	copy->Release();
	surface->Release();
	return RENDER_SUCCEED(render);
}

// RENDER DRAWING.
static int drawTexture(render_state *render,IDirect3DTexture9 *texture,float x,float y,float w,float h,DWORD argb,float x1,float y1,float x2,float y2){
	render_vertex *vertices;
	int index;
	
	vertices = render->vertices;
	vertices[0].x = (x + w) * render->width;
	vertices[0].y = (y + h) * render->height;
	vertices[0].u = x2;
	vertices[0].v = y2;
	vertices[1].x = x * render->width;
	vertices[1].y = (y + h) * render->height;
	vertices[1].u = x1;
	vertices[1].v = y2;
	vertices[2].x = x * render->width;
	vertices[2].y = y * render->height;
	vertices[2].u = x1;
	vertices[2].v = y1;
	vertices[3].x = (x + w) * render->width;
	vertices[3].y = y * render->height;
	vertices[3].u = x2;
	vertices[3].v = y1;
	for(index = 0;index < 4;index++)
		vertices[index].argb = argb;
	if(render->device->SetTexture(0,texture) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	if(render->device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,g_quadindices,D3DFMT_INDEX32,vertices,sizeof(render_vertex)) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	return RENDER_SUCCEED(render);
}
static int drawTexture2(render_state *render,IDirect3DTexture9 *texture,float x,float y,float w,float h,DWORD argb,float x1,float y1,float x2,float y2,float spin){
	render_vertex *vertices,*vertex;
	float length,angle;
	int index;
	
	// centered and rotated
	x *= render->width;
	y *= render->height;
	w *= render->width * 0.5f;
	h *= render->height * 0.5f;
	vertices = render->vertices;
	vertices[0].x = w;
	vertices[0].y = h;
	vertices[0].u = x2;
	vertices[0].v = y2;
	vertices[1].x = -w;
	vertices[1].y = h;
	vertices[1].u = x1;
	vertices[1].v = y2;
	vertices[2].x = -w;
	vertices[2].y = -h;
	vertices[2].u = x1;
	vertices[2].v = y1;
	vertices[3].x = w;
	vertices[3].y = -h;
	vertices[3].u = x2;
	vertices[3].v = y1;
	for(index = 0;index < 4;index++){
		vertex = vertices + index;
		length = sqrt(vertex->x*vertex->x+vertex->y*vertex->y);
		angle = atan2(-vertex->x,vertex->y) + spin;
		vertex->x = x - sin(angle) * length;
		vertex->y = y + cos(angle) * length;
		vertex->argb = argb;
	}
	if(render->device->SetTexture(0,texture) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	if(render->device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,g_quadindices,D3DFMT_INDEX32,vertices,sizeof(render_vertex)) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	return RENDER_SUCCEED(render);
}
RENDER_API int drawRenderRectangle(render_state *render,float x,float y,float w,float h,DWORD argb){
	render_vertex *vertices;
	int index;
	
	if(!w || !h)
		return RENDER_SUCCEED(render);
	vertices = render->vertices;
	vertices[0].x = (x + w) * render->width;
	vertices[0].y = (y + h) * render->height;
	vertices[1].x = x * render->width;
	vertices[1].y = (y + h) * render->height;
	vertices[2].x = x * render->width;
	vertices[2].y = y * render->height;
	vertices[3].x = (x + w) * render->width;
	vertices[3].y = y * render->height;
	for(index = 0;index < 4;index++)
		vertices[index].argb = argb;
	render->device->SetTexture(0,NULL);
	if(render->device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,g_quadindices,D3DFMT_INDEX32,vertices,sizeof(render_vertex)) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	return RENDER_SUCCEED(render);
}
RENDER_API int drawRenderTextureUV(render_state *render,render_texture *texture,float x,float y,float w,float h,DWORD argb,float x1,float y1,float x2,float y2){
	if(w && h)
		return drawTexture(render,texture->texture,x,y,w,h,argb,x1,y1,x2,y2);
	return RENDER_SUCCEED(render);
}
RENDER_API int drawRenderTextureUV2(render_state *render,render_texture *texture,float x,float y,float w,float h,DWORD argb,float x1,float y1,float x2,float y2,float spin){
	if(w && h)
		return drawTexture2(render,texture->texture,x,y,w,h,argb,x1,y1,x2,y2,spin);
	return RENDER_SUCCEED(render);
}
RENDER_API int clearRenderDisplay(render_state *render){
	if(render->device->Clear(0,NULL,D3DCLEAR_TARGET,0xFF000000,0.0f,0) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	return RENDER_SUCCEED(render);
}

// RENDER TEXT DRAWING.
static void getRenderStringPos(unsigned long flags,float x,float y,float w,float h,float *rx,float *ry){
	if(flags & RENDER_TEXT_CENTER)
		x -= w * 0.5f;
	else if(flags & RENDER_TEXT_RIGHT)
		x -= w;
	if(flags & RENDER_TEXT_DRAW_TEXT_UPWARDS)
		y -= h;
	else if(flags & RENDER_TEXT_VERTICALLY_CENTER)
		y -= h * 0.5f;
	*rx = x;
	*ry = y;
}
static void getRenderStringClipping(unsigned long flags,render_text_draw *args,float *width,float *height,float *x1,float *y1,float *x2,float *y2){
	float clipw,cliph;
	
	*x1 = 0.0f;
	*y1 = 0.0f;
	*x2 = 1.0f;
	*y2 = 1.0f;
	clipw = *width - args->width;
	cliph = *height - args->height;
	if(flags & RENDER_TEXT_CLIP_WIDTH && clipw > 0.0f){
		if(flags & RENDER_TEXT_CENTER)
			*x1 += (clipw * 0.5f) / *width, *x2 -= (clipw * 0.5f) / *width;
		else if(flags & RENDER_TEXT_RIGHT)
			*x1 += clipw / *width;
		else
			*x2 -= clipw / *width;
		*width -= clipw;
	}
	if(flags & RENDER_TEXT_CLIP_HEIGHT && cliph > 0.0f){
		if(flags & RENDER_TEXT_VERTICALLY_CENTER)
			*y1 += (cliph * 0.5f) / *height, *y2 -= (cliph * 0.5f) / *height;
		else if(flags & RENDER_TEXT_DRAW_TEXT_UPWARDS)
			*y1 += cliph / *height;
		else
			*y2 -= cliph / *height;
		*height -= cliph;
	}
}
static int drawRenderStringTexture(unsigned long flags,render_state *render,IDirect3DTexture9 *texture,float x,float y,float w,float h,float size,DWORD argb,DWORD argb2,float x1,float y1,float x2,float y2){
	float shadowx,shadowy,outlinex,outliney;
	DWORD black;
	int alpha;
	
	shadowx = (shadowy = size*SHADOW_SIZE) / ((float) render->width / render->height);
	outlinex = (outliney = size*OUTLINE_SIZE) / ((float) render->width / render->height);
	alpha = (argb >> 24) * OUTLINE_ALPHA_MULT;
	black = (alpha << 24)  | (argb2 & 0x00FFFFFF);
	if(flags & RENDER_TEXT_SHADOW && !drawTexture(render,texture,x+shadowx,y+shadowy,w,h,black,x1,y1,x2,y2))
		return 0;
	if(flags & RENDER_TEXT_OUTLINE){
		if(!drawTexture(render,texture,x+outlinex,y+outliney,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x-outlinex,y+outliney,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x-outlinex,y-outliney,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x+outlinex,y-outliney,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x+outlinex,y,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x,y+outliney,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x-outlinex,y,w,h,black,x1,y1,x2,y2))
			return 0;
		if(!drawTexture(render,texture,x,y-outliney,w,h,black,x1,y1,x2,y2))
			return 0;
	}
	if(!drawTexture(render,texture,x,y,w,h,argb,x1,y1,x2,y2))
		return 0;
	return 1;
}
RENDER_API int drawRenderString(render_state *render,font_renderer_font *font,render_text_draw *args,float *advance_x,float *advance_y){
	IDirect3DTexture9 *texture;
	float x,y,cw,ch;
	float x1,y1,x2,y2;
	float width,height;
	unsigned long flags;
	
	flags = args->flags;
	if(!args->size || !*args->text || ((flags & RENDER_TEXT_FIXED_LENGTH) && !args->length)){
		if(advance_x)
			*advance_x = 0.0f;
		if(advance_y)
			*advance_y = 0.0f;
		return 1; // no text/size does nothing but is still considered successful.
	}
	texture = drawFontRendererTexture(render->fontrender,font,args,&width,&height,&render->status);
	if(!texture){
		if(advance_x)
			*advance_x = 0.0f;
		if(advance_y)
			*advance_y = 0.0f;
		#ifdef SUPPRESS_TEXT_LIMIT_ERROR
		if(render->status == RENDER_FAIL_TEXT_LIMIT_REACHED)
			return 1;
		#endif
		return render->status == RENDER_FAIL_TEXT_NOT_BIG_ENOUGH; // not being big enough to draw is still considered successful.
	}
	getRenderStringClipping(flags,args,&width,&height,&x1,&y1,&x2,&y2);
	if(~flags & RENDER_TEXT_ONLY_MEASURE_TEXT){
		getRenderStringPos(flags,args->x,args->y,width,height,&x,&y);
		if(width && height && !drawRenderStringTexture(flags,render,texture,x,y,width,height,args->size,args->color,args->color2,x1,y1,x2,y2))
			return 0;
	}
	if(advance_x)
		*advance_x = width;
	if(advance_y)
		*advance_y = height;
	return 1;
}
RENDER_API font_renderer_font* createRenderFont(render_state *render,void *file,const char *name){
	// render_font is just font_renderer_font.
	return createFontRendererFont(render->fontrender,file,name,&render->status);
}
RENDER_API void destroyRenderFont(render_state *render,font_renderer_font *font){
	destroyFontRendererFont(render->fontrender,font);
}
RENDER_API unsigned getRenderStringCacheCount(render_state *render,int *created){
	return getFontRendererCacheCount(render->fontrender,created);
}

// RENDER CYCLE.
RENDER_API int startRender(render_state *render){
	IDirect3DDevice9 *device;
	D3DVIEWPORT9 rect;
	DWORD *backup;
	
	device = render->device;
	if(render->active || device->TestCooperativeLevel() != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_NOT_READY_FOR_DRAW);
	if(device->GetViewport(&rect) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	#ifdef ASSERT_VIEWPOINT
	if(assertViewport(rect.Width,rect.Height))
		return RENDER_FAIL(render,RENDER_FAIL_VIEWPORT_ASSERT);
	#endif
	render->width = rect.Width;
	render->height = rect.Height;
	#ifdef USE_STATE_BLOCKS
	/* A UI pass must restore the complete native pipeline, including streams,
	   indices, declarations, textures and shader constants. Draw*PrimitiveUP
	   clears stream/index bindings, which RenderWare's cache does not expect.
	   Release the snapshot at the end of this pass so it cannot retain default-
	   pool game buffers across a device reset. */
	if(device->CreateStateBlock(D3DSBT_ALL,&render->backup) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	#else
	backup = render->backup;
	device->GetRenderState(D3DRS_ZENABLE,backup++);
	device->GetRenderState(D3DRS_ALPHABLENDENABLE,backup++);
	device->GetRenderState(D3DRS_SRCBLEND,backup++);
	device->GetRenderState(D3DRS_DESTBLEND,backup++);
	device->GetSamplerState(0,D3DSAMP_ADDRESSU,backup++);
	device->GetSamplerState(0,D3DSAMP_ADDRESSV,backup++);
	device->GetTextureStageState(0,D3DTSS_COLOROP,backup++);
	device->GetTextureStageState(0,D3DTSS_ALPHAOP,backup++);
	device->GetTextureStageState(0,D3DTSS_ALPHAARG1,backup++);
	device->GetTextureStageState(0,D3DTSS_ALPHAARG2,backup++);
	device->GetVertexShader(&render->vbackup);
	device->GetPixelShader(&render->pbackup);
	device->GetFVF(backup);
	#endif
	render->active = 1;
	device->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
	device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
	device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
	device->SetTextureStageState(0,D3DTSS_COLOROP,4);
	device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
	device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
	device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
	device->SetVertexShader(NULL);
	device->SetPixelShader(NULL);
	device->SetFVF(FVF_FLAGS);
	return RENDER_SUCCEED(render);
}
RENDER_API void finishRender(render_state *render){
	IDirect3DDevice9 *device;
	DWORD *backup;
	
	if(render->active){
		#ifdef USE_STATE_BLOCKS
		render->backup->Apply();
		render->backup->Release();
		render->backup = NULL;
		#else
		device = render->device;
		backup = render->backup;
		device->SetVertexShader(render->vbackup);
		device->SetPixelShader(render->pbackup);
		device->SetFVF(*backup++);
		#endif
		render->active = 0;
	}
}
RENDER_API int startRenderDwriteCycle(render_state *render){
	D3DVIEWPORT9 rect;
	
	if(render->device->GetViewport(&rect) != D3D_OK)
		return RENDER_FAIL(render,RENDER_FAIL_D3D9);
	#ifdef ASSERT_VIEWPOINT
	if(assertViewport(rect.Width,rect.Height))
		return RENDER_FAIL(render,RENDER_FAIL_VIEWPORT_ASSERT);
	#endif
	startFontRendererCycle(render->fontrender,rect.Width,rect.Height);
	return RENDER_SUCCEED(render);
}
RENDER_API void finishRenderDwriteCycle(render_state *render){
	finishFontRendererCycle(render->fontrender);
}
RENDER_API int isRenderActive(render_state *render){
	return render->active;
}
RENDER_API float getRenderAspect(render_state *render){
	return (float) render->width / render->height;
}
RENDER_API void getRenderResolution(render_state *render,int *width,int *height){
	*width = render->width;
	*height = render->height;
}

// RENDER STATE.
RENDER_API render_state* createRender(IDirect3DDevice9 *device){
	D3DVIEWPORT9 rect;
	render_state *render;
	render_vertex *vertex;
	int index;
	
	render = (render_state*) malloc(sizeof(render_state));
	if(!render)
		return NULL;
	render->fontrender = createFontRenderer(device,NULL);
	if(!render->fontrender){
		free(render);
		return NULL;
	}
	if(device->TestCooperativeLevel() != D3D_OK || device->GetViewport(&rect) != D3D_OK){
		destroyFontRenderer(render->fontrender);
		free(render);
		return NULL;
	}
	#ifdef ASSERT_VIEWPOINT
	if(assertViewport(rect.Width,rect.Height)){
		destroyFontRenderer(render->fontrender);
		free(render);
		return NULL;
	}
	#endif
	#ifdef USE_STATE_BLOCKS
	render->backup = NULL;
	#endif
	for(index = 0;index < 4;index++){
		vertex = render->vertices + index;
		vertex->z = 0.0f;
		vertex->w = 1.0f;
	}
	render->device = device;
	render->first_texture = NULL;
	render->width = rect.Width;
	render->height = rect.Height;
	render->active = 0;
	render->status = RENDER_OK;
	render->cbuffer = NULL;
	return render;
}
RENDER_API void destroyRender(render_state *render){
	int index;
	
	destroyFontRenderer(render->fontrender);
	#ifdef USE_STATE_BLOCKS
	if(render->backup){
		if(render->active)render->backup->Apply();
		render->backup->Release();
	}
	#endif
	destroyAllRenderTextures(render);
	if(render->cbuffer)
		free(render->cbuffer);
	free(render);
}

// RENDER ERRORS.
RENDER_API int getRenderError(render_state *render){
	return render->status;
}
RENDER_API const char* getRenderErrorString(render_state *render){
	int status;
	
	status = render->status;
	if(render->cbuffer)
		free(render->cbuffer);
	render->cbuffer = (char*) malloc(256);
	if(!render->cbuffer)
		return FALLBACK_ERROR_STRING;
	if(status == RENDER_OK)
		sprintf(render->cbuffer,"renderer is ok");
	else if(status == RENDER_FAIL_UNK)
		sprintf(render->cbuffer,"unknown error");
	else if(status == RENDER_FAIL_GDI)
		sprintf(render->cbuffer,"unknown gdi error");
	else if(status == RENDER_FAIL_D3D9)
		sprintf(render->cbuffer,"unknown d3d9 error");
	else if(status == RENDER_FAIL_DWRITE)
		sprintf(render->cbuffer,"unknown dwrite error");
	else if(status == RENDER_FAIL_MEMORY)
		sprintf(render->cbuffer,"failed to allocate memory");
	else if(status == RENDER_FAIL_READ_FILE)
		sprintf(render->cbuffer,"failed to read file");
	else if(status == RENDER_FAIL_MISSING_FILE)
		sprintf(render->cbuffer,"missing file");
	else if(status == RENDER_FAIL_INVALID_FILE)
		sprintf(render->cbuffer,"invalid file");
	else if(status == RENDER_FAIL_UNSUPPORTED_FILE)
		sprintf(render->cbuffer,"unsupported file");
	else if(status == RENDER_FAIL_NOT_READY_FOR_DRAW)
		sprintf(render->cbuffer,"not ready to draw");
	else if(status == RENDER_FAIL_BAD_CALL_ARGUMENTS)
		sprintf(render->cbuffer,"bad arguments for call");
	else if(status == RENDER_FAIL_TEXT_LIMIT_REACHED)
		sprintf(render->cbuffer,"text draw limit reached");
	else if(status == RENDER_FAIL_TEXT_NOT_BIG_ENOUGH)
		sprintf(render->cbuffer,"text isn't big enough to draw");
	else if(status == RENDER_FAIL_DW_GET_GDI_INTEROP)
		sprintf(render->cbuffer,"dwrite error: failed to get interop");
	else if(status == RENDER_FAIL_DW_CREATE_BITMAP)
		sprintf(render->cbuffer,"dwrite error: failed to create bitmap");
	else if(status == RENDER_FAIL_DW_TEXT_FORMAT)
		sprintf(render->cbuffer,"dwrite error: failed to create format");
	else if(status == RENDER_FAIL_DW_TEXT_LAYOUT)
		sprintf(render->cbuffer,"dwrite error: failed to create layout");
	else if(status == RENDER_FAIL_DW_FACTORY)
		sprintf(render->cbuffer,"dwrite error: failed to create factory");
	else if(status == RENDER_FAIL_DW_MONITOR)
		sprintf(render->cbuffer,"dwrite error: failed to query monitor");
	else if(status == RENDER_FAIL_DW_DRAW)
		sprintf(render->cbuffer,"dwrite error: failed to draw text");
	else if(status == RENDER_FAIL_DIF_FORMATS)
		sprintf(render->cbuffer,"different texture formats");
	else if(status == RENDER_FAIL_CREATE_TEXTURE)
		sprintf(render->cbuffer,"failed to create texture");
	else if(status == RENDER_FAIL_VIEWPORT_ASSERT)
		sprintf(render->cbuffer,"invalid viewport");
	else
		sprintf(render->cbuffer,"unknown error code: %d",status);
	return render->cbuffer;
}
