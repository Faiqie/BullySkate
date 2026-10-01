/* DERPY'S SCRIPT SERVER: SCRIPT CONSOLE
	
	server compatibility layer for the client's console
	
*/

#include <dsl/dsl.h>
#include <stdarg.h>
#include <time.h>

// Types.
typedef struct script_console{
	FILE *output_file;
}script_console;

// Core.
script_console* createScriptConsole(){
	script_console *sc;
	FILE *file;
	
	sc = malloc(sizeof(script_console));
	if(!sc)
		return NULL;
	sc->output_file = NULL;
	return sc;
}
void destroyScriptConsole(script_console *sc){
	if(sc->output_file)
		fclose(sc->output_file);
	free(sc);
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
	#ifdef _WIN32
	HANDLE console;
	#endif
	
	if(!*str)
		return 1; // success for an empty string
	if(sc && sc->output_file)
		logConsoleOutput(sc->output_file,type,str);
	#ifdef _WIN32
	if((console = GetStdHandle(STD_OUTPUT_HANDLE)) != INVALID_HANDLE_VALUE){
		WriteConsole(console,"\r",1,NULL,NULL);
		if(type == CONSOLE_OUTPUT)
			SetConsoleTextAttribute(console,FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
		else if(type == CONSOLE_ERROR)
			SetConsoleTextAttribute(console,FOREGROUND_RED);
		else if(type == CONSOLE_WARNING)
			SetConsoleTextAttribute(console,FOREGROUND_RED|FOREGROUND_GREEN);
		else
			SetConsoleTextAttribute(console,FOREGROUND_BLUE|FOREGROUND_INTENSITY);
		WriteConsole(console,str,strlen(str),NULL,NULL);
		SetConsoleTextAttribute(console,FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
		WriteConsole(console,"\n> ",3,NULL,NULL);
	}else{
		printf("\r%s\n> ",str);
		fflush(stdout);
	}
	#else
	printf("%s\n",str);
	fflush(stdout);
	#endif
	return 0;
}
int setScriptConsoleLogging(script_console *sc,const char *path){
	if(sc->output_file)
		fclose(sc->output_file);
	return (sc->output_file = fopen(path,"wb")) != NULL;
}
void clearScriptConsole(script_console *sc){
	#ifdef DSL_SYSTEM_CLS
	system(DSL_SYSTEM_CLS);
	#ifdef _WIN32
	printf("> ");
	#endif
	#else
	#ifdef _WIN32
	printf("console clearing unsupported\n> ");
	#else
	printf("console clearing unsupported\n");
	#endif
	#endif
	fflush(stdout);
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