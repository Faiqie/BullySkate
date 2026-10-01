/* DERPY'S SCRIPT SERVER: SCRIPT CONSOLE
	
	server compatibility layer for the client's console
	
*/

#ifndef DSL_SV_CONSOLE_H
#define DSL_SV_CONSOLE_H

// PRINT TYPES
#define CONSOLE_OUTPUT 0
#define CONSOLE_ERROR 1
#define CONSOLE_WARNING 2
#define CONSOLE_SPECIAL 3 // doesn't really mean anything specific, just another color of output

// TYPES
typedef struct script_console script_console;

#ifdef __cplusplus
extern "C" {
#endif

// PRINT FUNCTIONS
#define printConsoleOutput(sc,...) printConsoleFormatted(sc,CONSOLE_OUTPUT,__VA_ARGS__)
#define printConsoleError(sc,...) printConsoleFormatted(sc,CONSOLE_ERROR,__VA_ARGS__)
#define printConsoleWarning(sc,...) printConsoleFormatted(sc,CONSOLE_WARNING,__VA_ARGS__)
#define printConsoleSpecial(sc,...) printConsoleFormatted(sc,CONSOLE_SPECIAL,__VA_ARGS__)
int printConsoleRaw(script_console *sc,int type,const char *str); // type is one of the CONSOLE_* constants, non-zero is success (allocation).
void printConsoleFormatted(script_console *sc,int type,const char *fmt,...);

// CONSOLE CONTROLLER
script_console* createScriptConsole(void);
void destroyScriptConsole(script_console *sc);
int setScriptConsoleLogging(script_console *sc,const char *path);
void clearScriptConsole(script_console *sc);

#ifdef __cplusplus
}
#endif

#endif