/* DERPY'S SCRIPT LOADER: SCRIPT CONSOLE
	
	this is the script console that can be toggled with ~ (varies on region)
	the print functions offer ways to print raw or formatted strings
	and the console controller is only for use by dsl.c
	
*/

#ifndef DSL_CONSOLE_H
#define DSL_CONSOLE_H

#include "render.h"

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
script_console* createScriptConsole(render_state *render,const char *sigpngpath,const char *font,float size,int custom,DWORD bgargb);
void destroyScriptConsole(script_console *sc);
int setScriptConsoleLogging(script_console *sc,const char *path);
int drawScriptConsole(script_console *sc); // if zero, renderer failed.
void clearScriptConsole(script_console *sc);
void refocusScriptConsole(script_console *sc);
int sendScriptConsoleKeystroke(script_console *sc,int vk); // if input was submitted, non-zero is returned.
const char* getScriptConsoleText(script_console *sc); // use when send keystroke returns non-zero.

#ifdef __cplusplus
}
#endif

#endif