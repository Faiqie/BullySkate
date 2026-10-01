/* DERPY'S SCRIPT LOADER: SCRIPT CONSOLE
	
	this is the script console that can be toggled with ~ (varies on region)
	the print functions offer ways to print raw or formatted strings
	and the console controller is only for use by dsl.c
	
*/

#include <dsl/dsl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define CONSOLE_FONT "Microsoft JhengHei UI Light" // default in case no custom font is given
#define CONSOLE_IN_MAX 255 // chars for string (-1 total bytes)
#define CONSOLE_OUT_MAX 1024 // whole messages (minimum 2, if not defined it will not be limited)
#define CONSOLE_HISTORY_SIZE 16

// Types.
typedef struct output_message{
	int type;
	char *str;
	unsigned size;
	struct output_message *next;
}output_message;
typedef struct script_console{
	render_state *render;
	render_font *font;
	render_texture *signature;
	DWORD background_argb;
	int custom_signature;
	float textsize;
	char input[CONSOLE_IN_MAX + 1];
	int input_count;
	char *input_restore[CONSOLE_HISTORY_SIZE];
	int input_restores;
	int restore_index;
	output_message *output;
	int output_count;
	int output_offset;
	FILE *output_file;
}script_console;

// Core.
script_console* createScriptConsole(render_state *render,const char *sigpngpath,const char *font,float size,int custom,DWORD bgargb){
	script_console *sc;
	FILE *file;
	
	sc = malloc(sizeof(script_console));
	if(!sc)
		return NULL;
	sc->signature = NULL;
	if(sc->render = render){
		sc->font = createRenderFont(render,NULL,font ? font : CONSOLE_FONT);
		if(!sc->font){
			free(sc);
			return NULL;
		}
		if(sigpngpath && (file = fopen(sigpngpath,"rb"))){
			sc->signature = createRenderTexture(render,file);
			fclose(file);
		}
	}
	sc->background_argb = bgargb;
	sc->custom_signature = custom;
	sc->textsize = 0.0175f * size;
	*sc->input = 0;
	sc->input_count = 0;
	sc->input_restores = 0;
	sc->restore_index = -1;
	sc->output = NULL;
	sc->output_count = 0;
	sc->output_offset = 0;
	sc->output_file = NULL;
	return sc;
}
void destroyScriptConsole(script_console *sc){
	output_message *msg,*next;
	
	next = sc->output;
	while(msg = next){
		next = msg->next;
		free(msg->str);
		free(msg);
	}
	while(sc->input_restores)
		free(sc->input_restore[--sc->input_restores]);
	if(sc->render){
		if(sc->signature)
			destroyRenderTexture(sc->render,sc->signature);
		destroyRenderFont(sc->render,sc->font);
	}
	if(sc->output_file)
		fclose(sc->output_file);
	free(sc);
}
int setScriptConsoleLogging(script_console *sc,const char *path){
	if(sc->output_file)
		fclose(sc->output_file);
	return (sc->output_file = fopen(path,"wb")) != NULL;
}
static int drawOutput(script_console *sc){
	output_message *msg,*next;
	render_text_draw drawtext;
	float ar,height;
	int offset;
	
	ar = getRenderAspect(sc->render);
	if(!drawRenderRectangle(sc->render,-0.1f,0.7f,1.2f,0.3f,sc->background_argb)) // extra width because idk why there was a column of missing pixels on the left
		return 0;
	if(sc->signature){
		if(sc->custom_signature){
			if(!drawRenderTexture(sc->render,sc->signature,1.0f-0.3f/ar,0.7f,0.3f/ar,0.3f,0xFFFFFFFF)) // custom 1:1 signature image
				return 0;
		}else if(!drawRenderTexture(sc->render,sc->signature,1.0f-0.25f/ar,0.7f,0.25f/ar,0.25f,0x32C5960B)) // 002D56 (blue), C5960B (yellow)
			return 0;
	}
	memset(&drawtext,0,sizeof(render_text_draw));
	drawtext.flags = RENDER_TEXT_FIXED_LENGTH | RENDER_TEXT_WORD_WRAP | RENDER_TEXT_DRAW_TEXT_UPWARDS | RENDER_TEXT_CLIP_HEIGHT;
	drawtext.size = sc->textsize;
	drawtext.x = 0.01f / ar;
	drawtext.y = 0.95f;
	drawtext.width = 1.0f - drawtext.x * 2;
	drawtext.height = (drawtext.y - 0.7f) - 0.01f;
	next = sc->output;
	for(offset = sc->output_offset;next && offset;offset--)
		next = next->next;
	for(;msg = next;next = msg->next){
		if(msg->type == CONSOLE_OUTPUT)
			drawtext.color = 0xFFDFDFDF;
		else if(msg->type == CONSOLE_ERROR)
			drawtext.color = 0xFFF93939;
		else if(msg->type == CONSOLE_WARNING)
			drawtext.color = 0xFFFA7744;
		else
			drawtext.color = 0xFF00C0FF;
		drawtext.text = msg->str;
		drawtext.length = msg->size;
		if(!drawRenderString(sc->render,sc->font,&drawtext,NULL,&height))
			return 0;
		drawtext.y -= (height += 0.005f);
		if((drawtext.height -= height) <= 0)
			break;
	}
	return 1;
}
static int drawInput(script_console *sc,float start,float height){
	render_text_draw drawtext;
	float ar;
	
	ar = getRenderAspect(sc->render);
	drawtext.flags = RENDER_TEXT_FIXED_LENGTH | RENDER_TEXT_VERTICALLY_CENTER | RENDER_TEXT_CLIP_WIDTH;
	drawtext.text = sc->input;
	drawtext.length = sc->input_count;
	drawtext.color = 0xFFFFFFFF;
	drawtext.size = 0.0175f;
	drawtext.x = 0.015f / ar;
	drawtext.y = start + height * 0.5f;
	drawtext.width = 1.0f - drawtext.x * 2.0f;
	if(!drawRenderRectangle(sc->render,0.005f,start,0.99f,height,0xBB363636))
		return 0;
	if(!drawRenderString(sc->render,sc->font,&drawtext,NULL,&height))
		return 0;
	return 1;
}
int drawScriptConsole(script_console *sc){
	return drawOutput(sc) && drawInput(sc,0.96f,0.03f);
}
static void logConsoleOutput(FILE *file,int type,const char *str){
	struct tm *tm;
	time_t timer;
	
	time(&timer);
	tm = localtime(&timer);
	if(type == CONSOLE_OUTPUT || type == CONSOLE_SPECIAL)
		fprintf(file,"[%.2d:%.2d:%.2d] %s\r\n",tm->tm_hour,tm->tm_min,tm->tm_sec,str);
	else
		fprintf(file,"[%.2d:%.2d:%.2d] [%s] %s\r\n",tm->tm_hour,tm->tm_min,tm->tm_sec,type == CONSOLE_ERROR ? "ERROR" : "WARNING",str);
	fflush(file);
}
int printConsoleRaw(script_console *sc,int type,const char *str){
	output_message *msg,*next,*last;
	size_t bytes;
	char *copy;
	
	if(!*str)
		return 1; // success for an empty string
	if(sc->output_file)
		logConsoleOutput(sc->output_file,type,str);
	#ifdef CONSOLE_OUT_MAX
	if(sc->output_count < CONSOLE_OUT_MAX){ // at least 2
	#endif
		msg = malloc(sizeof(output_message));
		if(!msg)
			return 0;
		msg->next = sc->output;
		sc->output = msg;
		sc->output_count++;
		if(sc->output_offset)
			sc->output_offset++;
	#ifdef CONSOLE_OUT_MAX
	}else{
		for(msg = sc->output;next = msg->next;msg = next)
			last = msg; // (2nd to last)
		free(last->next->str); // remove last message's string
		last->next->next = sc->output; // take the last message and make it point to the first
		sc->output = last->next; // new first message is the last message
		last->next = NULL; // and the 2nd to last no longer points to the last
		msg = sc->output;
		if(sc->output_offset && ++sc->output_offset >= sc->output_count)
			sc->output_offset = sc->output_count - 1;
	}
	#endif
	if(copy = malloc(bytes = strlen(str))){
		msg->str = memcpy(copy,str,bytes);
		msg->size = (unsigned) bytes;
		msg->type = type;
		return 1;
	}
	sc->output = msg->next;
	free(msg);
	return 0;
}
void clearScriptConsole(script_console *sc){
	output_message *msg,*next;
	
	next = sc->output;
	while(msg = next){
		next = msg->next;
		free(msg->str);
		free(msg);
	}
	sc->output = NULL;
	sc->output_count = 0;
	sc->output_offset = 0;
}
void refocusScriptConsole(script_console *sc){
	sc->input_count = 0;
	sc->output_offset = 0;
	sc->restore_index = -1;
}
static void fillInputWithClipboard(script_console *sc){
	HANDLE clipboard;
	const char *str;
	char c;
	
	if(OpenClipboard(NULL)){
		if(clipboard = GetClipboardData(CF_TEXT)){
			for(str = clipboard;isprint(c = *str);str++)
				if(sc->input_count < CONSOLE_IN_MAX)
					sc->input[sc->input_count++] = c;
			sc->input[sc->input_count] = 0;
			sc->restore_index = -1;
		}
		CloseClipboard();
	}
}
static void trimTrailingInput(script_console *sc){
	unsigned count;
	char *str;
	
	count = sc->input_count;
	str = sc->input;
	while(count && isspace(str[--count]))
		str[count] = 0;
}
int sendScriptConsoleKeystroke(script_console *sc,int key){
	unsigned char keyboard[256];
	char *restore;
	char ascii[2];
	int index;
	
	if(!GetKeyboardState(keyboard)){
		strcpy(sc->input,"keyboard failure");
		sc->input_count = strlen(sc->input);
	}else if(keyboard[VK_CONTROL] & 0x80){
		if(key == DIK_V)
			fillInputWithClipboard(sc);
	}else if(key == DIK_RETURN){
		if(sc->input_count){
			if(!sc->input_restores || strcmp(sc->input,sc->input_restore[sc->input_restores-1])){
				if(sc->input_restores == CONSOLE_HISTORY_SIZE){
					free(*sc->input_restore);
					sc->input_restores--;
					for(index = 0;index < sc->input_restores;index++)
						sc->input_restore[index] = sc->input_restore[index+1];
				}
				if(restore = malloc(sc->input_count + 1))
					sc->input_restore[sc->input_restores++] = strcpy(restore,sc->input);
			}
			trimTrailingInput(sc);
			sc->input_count = 0;
			sc->restore_index = -1;
			return 1;
		}
	}else if(key == DIK_ESCAPE){
		*sc->input = 0;
		sc->input_count = 0;
		sc->restore_index = -1;
	}else if(key == DIK_UP){
		if(sc->input_restores){
			if(sc->restore_index == -1 || sc->restore_index >= sc->input_restores)
				sc->restore_index = sc->input_restores - 1; // most recent message
			else if(sc->restore_index)
				sc->restore_index--; // one message older
			sc->input_count = strlen(strcpy(sc->input,sc->input_restore[sc->restore_index]));
		}
	}else if(key == DIK_DOWN){
		if(sc->input_restores && sc->restore_index != -1){
			if(++sc->restore_index >= sc->input_restores) // one message newer
				sc->restore_index = sc->input_restores - 1; // but limit to most recent
			sc->input_count = strlen(strcpy(sc->input,sc->input_restore[sc->restore_index]));
		}
	}else if(key == DIK_PRIOR){
		if(sc->output_count && ++sc->output_offset >= sc->output_count)
			sc->output_offset = sc->output_count - 1;
	}else if(key == DIK_NEXT){
		if(sc->output_offset)
			sc->output_offset--;
	}else if(key == DIK_BACK){
		if(sc->input_count)
			sc->input[--sc->input_count] = 0;
		sc->restore_index = -1;
	}else if(sc->input_count < CONSOLE_IN_MAX && (key = MapVirtualKeyA(key,MAPVK_VSC_TO_VK)) && ToAscii(key,0,keyboard,(LPWORD)ascii,0) == 1 && isprint(*ascii) && (sc->input_count || !isspace(*ascii))){
		sc->input[sc->input_count++] = *ascii;
		sc->input[sc->input_count] = 0;
		sc->restore_index = -1;
	}
	return 0;
}
const char* getScriptConsoleText(script_console *sc){
	return sc->input;
}

// Formatted.
void printConsoleFormatted(script_console *sc,int type,const char *fmt,...){
	va_list arguments;
	char *buffer;
	int size;
	
	va_start(arguments,fmt);
	size = vsnprintf(NULL,0,fmt,arguments);
	if(size >= 0 && (buffer = malloc(++size))){
		vsnprintf(buffer,size,fmt,arguments);
		printConsoleRaw(sc,type,buffer);
		free(buffer);
	}
	va_end(arguments);
}