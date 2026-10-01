/* DERPY'S SCRIPT LOADER: COMMAND PROCESSOR
	
	this is where commands are registered and processed
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>

#define DEFAULT_COMMAND "/" // optional default command
#define CMD_PER_LINE 6 // for /help

// Script command types.
typedef struct script_command{
	void (*cb)(void*,int,char**);
	void *arg;
	int rawargs;
	char *helptext;
	struct script_command *next;
}script_command;

// Insensitive strings.
int areScriptCommandNamesEqual(const char *a,const char *b){
	for(;*a || *b;a++,b++)
		if(tolower(*a) != tolower(*b))
			return 0;
	return 1;
}
static int areScriptCommandNamesEqual2(const char *a,const char *b,int count){ // count is for a
	for(;count;a++,b++,count--)
		if(tolower(*a) != tolower(*b))
			return 0;
	return !*b;
}

// Argument parsing.
static int getArguments(const char *text,int *argc_result,char ***argv_result){
	int argc,inarg,inquote,c;
	char **argv,*buffer;
	
	while(isspace(*text))
		text++;
	if(!*text){
		*argc_result = 0;
		*argv_result = NULL;
		return 1;
	}
	buffer = malloc(strlen(text) + 1);
	if(!buffer)
		return 0;
	strcpy(buffer,text);
	for(inquote = inarg = argc = 0;c = *text;text++)
		if(c == '\'' || c == '"')
			inquote = !inquote;
		else if(!inquote && isspace(c))
			inarg = 0;
		else if(!inarg)
			inarg = 1,argc++;
	if(!argc){
		*argc_result = 0;
		*argv_result = NULL;
		free(buffer);
		return 1;
	}
	argv = malloc((argc + 1) * sizeof(char*)); // extra pointer for the full buffer (just for freeing)
	if(!argv){
		free(buffer);
		return 0;
	}
	argv[argc] = buffer;
	for(inquote = inarg = argc = 0;c = *buffer;buffer++)
		if(c == '\'' || c == '"')
			inquote = !inquote,*buffer = 0;
		else if(!inquote && isspace(c))
			inarg = 0,*buffer = 0;
		else if(!inarg)
			inarg = 1,argv[argc++] = buffer;
	*argc_result = argc;
	*argv_result = argv;
	return 1;
}
static void freeArguments(int argc,char **argv){
	if(argv){
		free(argv[argc]);
		free(argv);
	}
}

// Remove command.
static void freeScriptCommand(script_command *cmd){
	if(cmd->helptext)
		free(cmd->helptext);
	free(cmd);
}
static void removeScriptCommand(script_cmdlist *list,const char *name){
	script_command *cmd,*next;
	
	cmd = *list;
	if(!cmd)
		return;
	if(areScriptCommandNamesEqual(name,(char*)(cmd+1))){
		*list = cmd->next;
		freeScriptCommand(cmd);
	}else
		for(;next = cmd->next;cmd = next)
			if(areScriptCommandNamesEqual(name,(char*)(next+1))){
				cmd->next = next->next;
				freeScriptCommand(next);
				return;
			}
}

// Run command.
static int runCommand(script_command *cmd,const char *text){
	char **args;
	int count;
	
	if(cmd->rawargs){
		while(isspace(*text))
			text++;
		if(*text){
			args = malloc(strlen(text) + 1);
			if(!args)
				return 0;
			strcpy((char*)args,text);
			(*cmd->cb)(cmd->arg,1,(char**)&args); // raw argument
			free(args);
		}else
			(*cmd->cb)(cmd->arg,0,NULL); // raw, but no argument
		return 1;
	}else if(!getArguments(text,&count,&args))
		return 0;
	(*cmd->cb)(cmd->arg,count,args); // normal argc / argv
	freeArguments(count,args);
	return 1;
}

// Help command.
static void sortScriptCommandList(script_cmdlist *list){
	script_command *first,*nextadd,*adding,*where,*after;
	
	first = *list;
	if(!first)
		return;
	nextadd = first->next;
	first->next = NULL; // new chain
	while(adding = nextadd){
		nextadd = adding->next;
		for(where = first;where;after = where,where = where->next)
			if(dslstrcmp((char*)(where+1),(char*)(adding+1)) > 0)
				break;
		if(where == first)
			first = adding; // new start of chain
		else
			after->next = adding; // not the start of the chain
		adding->next = where;
	}
	*list = first;
}
static void helpCommand(dsl_state *dsl,int argc,char **argv){
	script_command *cmd;
	size_t count,length,total;
	char *buffer;
	
	if(argc){
		for(cmd = *dsl->cmdlist;cmd;cmd = cmd->next)
			if(areScriptCommandNamesEqual(*argv,(char*)(cmd+1))){
				if(cmd->helptext)
					printConsoleRaw(dsl->console,CONSOLE_OUTPUT,cmd->helptext);
				else
					printConsoleRaw(dsl->console,CONSOLE_WARNING,"no help available for this command");
				return;
			}
		printConsoleRaw(dsl->console,CONSOLE_ERROR,"no help available for non-existant commands");
		return;
	}
	for(count = 0,cmd = *dsl->cmdlist;cmd;cmd = cmd->next)
		#ifdef DEFAULT_COMMAND
		if(!areScriptCommandNamesEqual(DEFAULT_COMMAND,(char*)(cmd+1)))
		#endif
			count += strlen((char*)(cmd+1)) + 2;
	if(buffer = malloc(count)){
		sortScriptCommandList(dsl->cmdlist);
		for(total = count = 0,cmd = *dsl->cmdlist;cmd;cmd = cmd->next){
			#ifdef DEFAULT_COMMAND
			if(areScriptCommandNamesEqual(DEFAULT_COMMAND,(char*)(cmd+1)))
				continue;
			#endif
			buffer[count++] = '/';
			strcpy(buffer+count,(char*)(cmd+1));
			count += strlen((char*)(cmd+1));
			if(++total % CMD_PER_LINE)
				buffer[count++] = '\t';
			else
				buffer[count++] = '\n';
		}
		if(count)
			buffer[count-1] = 0;
		printConsoleRaw(dsl->console,CONSOLE_OUTPUT,"Follow /help with another command name for more specific help.");
		printConsoleRaw(dsl->console,CONSOLE_OUTPUT,buffer);
		free(buffer);
	}else
		printConsoleRaw(dsl->console,CONSOLE_ERROR,"failed to help");
}

// Command api.
int setScriptCommandEx(script_cmdlist *list,const char *name,const char *help,void (*func)(void*,int,char**),void *arg,int raw){
	script_command *cmd;
	
	if(!func){
		removeScriptCommand(list,name);
		return 1;
	}
	for(cmd = *list;cmd;cmd = cmd->next)
		if(areScriptCommandNamesEqual(name,(char*)(cmd+1))){
			cmd->cb = func;
			cmd->arg = arg;
			strcpy((char*)(cmd+1),name); // just to update casing
			return 1;
		}
	cmd = malloc(sizeof(script_command) + strlen(name) + 1);
	if(!cmd)
		return 0;
	strcpy((char*)(cmd+1),name);
	cmd->cb = func;
	cmd->arg = arg;
	cmd->rawargs = raw;
	if(help){
		if(cmd->helptext = malloc(strlen(help)+1)) // very unlikely it will fail and if it does then we will just discard the help text
			strcpy(cmd->helptext,help);
	}else
		cmd->helptext = NULL;
	cmd->next = *list;
	*list = cmd;
	return 1;
}
int doesScriptCommandExist(script_cmdlist *list,const char *name){
	script_command *cmd;
	
	for(cmd = *list;cmd;cmd = cmd->next)
		if(areScriptCommandNamesEqual(name,(char*)(cmd+1)))
			return 1;
	return 0;
}
int processScriptCommandLine(script_cmdlist *list,const char *text){
	script_command *cmd;
	int count;
	char c;
	
	while(*text == '/' || isspace(*text))
		text++;
	count = 0;
	while((c = text[count]) && !isspace(c))
		count++;
	for(cmd = *list;cmd;cmd = cmd->next)
		if(areScriptCommandNamesEqual2(text,(char*)(cmd+1),count))
			return runCommand(cmd,text+count);
	#ifdef DEFAULT_COMMAND
	if(count){
		for(cmd = *list;cmd;cmd = cmd->next)
			if(areScriptCommandNamesEqual(DEFAULT_COMMAND,(char*)(cmd+1)))
				return runCommand(cmd,text);
	}
	#endif
	return 0;
}
void destroyScriptCommandList(script_cmdlist *list){
	script_command *cmd,*next;
	
	for(cmd = *list;cmd;cmd = next){
		next = cmd->next;
		free(cmd);
	}
	free(list);
}
script_cmdlist* createScriptCommandList(dsl_state *dsl){
	script_cmdlist *list;
	
	list = malloc(sizeof(script_command*));
	if(!list)
		return NULL;
	*list = NULL;
	setScriptCommandEx(list,"help",TEXT_HELP_HELP,&helpCommand,dsl,1);
	return list;
}