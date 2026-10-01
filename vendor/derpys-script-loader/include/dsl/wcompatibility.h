/* DERPY'S SCRIPT LOADER: WINDOWS COMPATIBILITY
	
	DSL was designed for Windows as that is what the game runs on
	this file provides some compatibility for compiling the server on other platforms
	
*/

#ifndef DSL_WCOMPATIBILITY_H
#define DSL_WCOMPATIBILITY_H

// DEPENDENCIES
#include <unistd.h>

// DEFINES
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

// MACROS
#define closesocket(s) close(s)

// TYPES
typedef unsigned DWORD;
typedef int SOCKET;

#endif