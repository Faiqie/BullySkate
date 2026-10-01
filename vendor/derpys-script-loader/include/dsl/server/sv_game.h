/* DERPY'S SCRIPT SERVER: SCRIPT CONSOLE
	
	server compatibility layer for game.h
	
*/

#ifndef DSL_SV_GAME_H
#define DSL_SV_GAME_H

extern struct dsl_state *g_dsl;

#define getGameTimer() (g_dsl ? g_dsl->last_frame : 0)

#endif