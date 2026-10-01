/* DERPY'S SCRIPT LOADER: MAIN
	
	this is the entry point of the mod (DllMain)
	it's responsible for patching the game and controlling dsl.c
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#include <time.h>
#include <signal.h>
#endif

#define CONSOLE_HISTORY 16
#define CONSOLE_BUFFER_SIZE 4096
//#define W32_CUT_SLEEP 10 // if defined, Sleep time will be reduced by this time to account for the poor accuracy of Windows timing

// DSL CONTROLLER
dsl_state* openDsl(void *game,void *d3d9,void *data);
void closeDsl(dsl_state*);
void updateDslAfterScripts(dsl_state*);

// GLOBAL STATE
dsl_state *g_dsl = NULL;

// INPUT (WINDOWS)
#ifdef _WIN32
struct console_input{
	HANDLE input;
	char *buffer;
	unsigned index;
	char *history[CONSOLE_HISTORY];
	int h_count;
	int h_index;
};
static void addConsoleHistory(struct console_input *ci){
	char *add;
	int shift;
	
	add = malloc(ci->index+1);
	if(!add)
		return;
	shift = ci->h_count;
	if(shift == CONSOLE_HISTORY)
		free(ci->history[--shift]); // get rid of oldest if we have the maximum amount
	else
		ci->h_count++;
	for(;shift;shift--)
		ci->history[shift] = ci->history[shift-1]; // shift everything up
	*ci->history = memcpy(add,ci->buffer,ci->index+1); // add the newest
}
static void startConsoleInput(struct console_input *ci,dsl_state *dsl){
	if(getConfigBoolean(dsl->config,"console_no_input")){
		ci->input = INVALID_HANDLE_VALUE;
		return;
	}
	ci->input = GetStdHandle(STD_INPUT_HANDLE);
	if(ci->input == INVALID_HANDLE_VALUE)
		return;
	FlushConsoleInputBuffer(ci->input);
	ci->buffer = malloc(CONSOLE_BUFFER_SIZE);
	if(!ci->buffer)
		ci->input = INVALID_HANDLE_VALUE;
	ci->index = 0;
	ci->h_count = 0;
}
static void stopConsoleInput(struct console_input *ci){
	if(ci->input == INVALID_HANDLE_VALUE)
		return;
	while(ci->h_count)
		free(ci->history[--ci->index]);
	free(ci->buffer);
}
static int updateConsoleInput(struct console_input *ci){
	INPUT_RECORD ir;
	DWORD read;
	
	if(ci->input == INVALID_HANDLE_VALUE || WaitForSingleObject(ci->input,0))
		return 0;
	while(ReadConsoleInput(ci->input,&ir,1,&read) && read){
		if(ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown){
			if(ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN){
				printf("\r> %.*s\n",ci->index,ci->buffer);
				fflush(stdout);
				ci->buffer[ci->index] = 0;
				if(ci->index)
					addConsoleHistory(ci);
				ci->h_index = 0;
				ci->index = 0;
				return 1; // done!
			}else if(ir.Event.KeyEvent.wVirtualKeyCode == VK_BACK){
				printf("\r> %*s",ci->index,"");
				if(ci->index)
					ci->index--;
				printf("\r> %.*s",ci->index,ci->buffer);
			}else if(ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE){
				printf("\r> %*s",ci->index,"");
				ci->h_index = 0;
				ci->index = 0;
			}else if(ir.Event.KeyEvent.wVirtualKeyCode == VK_UP){
				printf("\r> %*s",ci->index,"");
				if(ci->h_index < ci->h_count){
					strcpy(ci->buffer,ci->history[ci->h_index++]);
					ci->index = strlen(ci->buffer);
				}
				printf("\r> %.*s",ci->index,ci->buffer);
			}else if(ir.Event.KeyEvent.wVirtualKeyCode == VK_DOWN){
				printf("\r> %*s",ci->index,"");
				if(ci->h_index && --ci->h_index){
					strcpy(ci->buffer,ci->history[ci->h_index-1]);
					ci->index = strlen(ci->buffer);
				}else
					ci->index = 0;
				printf("\r> %.*s",ci->index,ci->buffer);
			}else if(isprint(ir.Event.KeyEvent.uChar.AsciiChar) && ci->index < CONSOLE_BUFFER_SIZE - 1){
				ci->buffer[ci->index++] = ir.Event.KeyEvent.uChar.AsciiChar;
				printf("\r> %.*s",ci->index,ci->buffer);
			}
			fflush(stdout);
		}
		if(WaitForSingleObject(ci->input,0))
			return 0;
	}
	FlushConsoleInputBuffer(ci->input); // failed to read so just flush for next time
	return 0;
}
#endif

// INPUT (UNIX)
#ifndef _WIN32
struct console_input{
	char *buffer;
};
static void startConsoleInput(struct console_input *ci,dsl_state *dsl){
	if(getConfigBoolean(dsl->config,"console_no_input")){
		ci->buffer = NULL;
		return;
	}
	ci->buffer = malloc(CONSOLE_BUFFER_SIZE);
}
static void stopConsoleInput(struct console_input *ci){
	if(ci->buffer)
		free(ci->buffer);
}
static int updateConsoleInput(struct console_input *ci){
	struct timeval tv;
	fd_set read;
	char c;
	int i;
	
	if(!ci->buffer)
		return 0;
	FD_ZERO(&read);
	FD_SET(STDIN_FILENO,&read);
	memset(&tv,0,sizeof(struct timeval));
	if(select(STDIN_FILENO+1,&read,NULL,NULL,&tv) == -1 || !FD_ISSET(STDIN_FILENO,&read))
		return 0;
	i = 0;
	while(fread(&c,1,1,stdin) == 1){
		if(c == '\n'){
			ci->buffer[i] = 0;
			return 1;
		}
		ci->buffer[i++] = c;
	}
	return 0;
}
#endif

// TERMINATE
#ifndef _WIN32
static void handleSigterm(int sig){
	g_dsl->flags &= ~DSL_RUN_MAIN_LOOP;
}
static int setupSignals(){
	struct sigaction sa;
	
	memset(&sa,0,sizeof(struct sigaction));
	sa.sa_handler = &handleSigterm;
	return sigaction(SIGTERM,&sa,NULL);
}
#endif
static void quitCommand(void *arg,int argc,char **argv){
	g_dsl->flags &= ~DSL_RUN_MAIN_LOOP;
}

// TIMING
struct server_timer{
	DWORD started;
	DWORD ticks;
	DWORD hz;
};
#ifndef _WIN32
static DWORD getUnixClock(){
	struct timespec ts;
	
	if(clock_gettime(CLOCK_MONOTONIC,&ts))
		return 0; // TODO: add an actual error report
	return (DWORD)ts.tv_sec * 1000 + (DWORD)ts.tv_nsec / 1000000;
}
#endif
static void startServerTimer(struct server_timer *st,dsl_state *dsl){
	DWORD hz;
	
	hz = getConfigInteger(dsl->config,"server_hz");
	st->hz = hz ? hz : NET_DEFAULT_HZ;
	st->ticks = 0;
	#ifdef _WIN32
	st->started = GetTickCount();
	#else
	st->started = getUnixClock();
	#endif
}
static int updateServerTimer(struct server_timer *st,dsl_state *dsl){
	DWORD timer;
	DWORD goal;
	
	#ifdef _WIN32
	timer = GetTickCount() - st->started;
	#else
	timer = getUnixClock() - st->started;
	#endif
	goal = (timer * st->hz) / 1000 + 1;
	dsl->frame_time = timer - dsl->last_frame;
	dsl->last_frame = timer;
	if(goal == st->ticks){
		goal *= 1000;
		timer = goal / st->hz - timer;
		if(goal % st->hz)
			timer++;
		#ifdef _WIN32
		#ifdef W32_CUT_SLEEP
		if(timer > W32_CUT_SLEEP)
			Sleep(timer - W32_CUT_SLEEP);
		else
			Sleep(0);
		#else
		Sleep(timer);
		#endif
		#else
		if(timer >= 1000)
			sleep(timer);
		else
			usleep(timer*1000);
		#endif
		return 0;
	}
	if(dsl->flags & DSL_SHOW_TICK_WARNINGS && (timer = goal - st->ticks - 1))
		printConsoleWarning(dsl->console,"missed %u ticks",timer);
	if(goal < st->ticks){
		dsl->t_wrapped++;
		printConsoleWarning(dsl->console,"The server has been running for over %u days, consider restarting soon.",49*dsl->t_wrapped);
	}
	st->ticks = goal;
	return 1;
}

// MAIN
#ifdef _WIN32
static void setupTitle(){
	char buffer[32];
	
	sprintf(buffer,"derpy's script server %d",DSL_VERSION);
	SetConsoleTitle(buffer);
}
#endif
int main(int argc,char **argv){
	struct console_input ci;
	struct server_timer st;
	dsl_state *dsl;
	
	#ifdef _WIN32
	setupTitle();
	#else
	if(setupSignals()){
		printf("failed to setup signal handlers\n");
		fflush(stdout);
		return 1;
	}
	#endif
	dsl = openDsl(NULL,NULL,NULL);
	if(!dsl){
		#ifdef _WIN32
		printf("\r "); // remove the >
		fflush(stdout);
		#endif
		#ifdef DSL_SYSTEM_PAUSE
		system(DSL_SYSTEM_PAUSE);
		#endif
		return 1;
	}
	g_dsl = dsl;
	dsl->flags |= DSL_RUN_MAIN_LOOP;
	setScriptCommandEx(dsl->cmdlist,"quit",TEXT_HELP_QUIT,&quitCommand,NULL,1);
	startConsoleInput(&ci,dsl);
	startServerTimer(&st,dsl);
	while(dsl->flags & DSL_RUN_MAIN_LOOP)
		if(updateServerTimer(&st,dsl)){
			if(updateConsoleInput(&ci)){
				#ifdef _WIN32
				printf("> "); // just in case no console output replaces it
				fflush(stdout);
				#endif
				if(*ci.buffer && !processScriptCommandLine(dsl->cmdlist,ci.buffer))
					printConsoleError(dsl->console,"command does not exist");
			}
			updateNetworking(dsl,dsl->network);
			updateDslAfterScripts(dsl);
			updateNetworking2(dsl,dsl->network);
		}
	closeDsl(dsl);
	stopConsoleInput(&ci);
	#ifdef _WIN32
	printf("\rserver shutdown\n");
	#else
	printf("server shutdown\n");
	#endif
	fflush(stdout);
	return 0;
}