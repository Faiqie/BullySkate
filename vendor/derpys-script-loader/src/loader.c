/* DERPY'S SCRIPT LOADER: SCRIPT LOADER
	
	the file loader finds scripts and controls them using the manager
	it also handles general file access to support any way a collection may load files
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#include <dirent.h> // opendir / readdir / closedir
#endif

//#define DEBUG_SAFETY_CHECKS
#define NORMAL_GAME_PATH "Scripts/"

#define FE_NONE 0
#define FE_STANDARD 1
#define FE_ZIPPED 2
#define FE_DSL 3

// FILE ERRORS
static int fe_type;
static union{
	int standard;
	zip_error_t *zipped;
	const char *dsl;
}fe_code;

// FILE ACCESS
static char* getDslFilePath(loader_collection *lc,const char *name){
	//const char *prefix;
	int folder;
	char *path;
	
	if(lc){
		/*
		#ifdef DSL_SERVER_VERSION
		prefix = DSL_SCRIPTS_PATH;
		#else
		prefix = lc->flags & LOADER_NETWORK ? DSL_SERVER_SCRIPTS_PATH : DSL_SCRIPTS_PATH;
		#endif
		*/
		if(folder = lc->flags & LOADER_FOLDER)
			path = malloc(strlen(lc->prefix) + strlen((char*)(lc+1)) + strlen(name) + 2);
		else
			path = malloc(strlen(lc->prefix) + strlen(name) + 1);
	}else{
		folder = 0;
		path = malloc(sizeof(NORMAL_GAME_PATH) + strlen(name));
	}
	if(!path)
		return NULL;
	if(lc)
		strcpy(path,lc->prefix);
	else
		strcpy(path,NORMAL_GAME_PATH);
	if(folder){
		strcat(path,(char*)(lc+1));
		strcat(path,"/");
	}
	strcat(path,name);
	return path;
}
static int openDslFileZipped(dsl_file *f,loader_collection *lc,const char *name,const char *mode,long *size){
	zip_stat_t stats;
	long index;
	
	#ifndef DSL_DISABLE_ZIP_FILES
	if(*mode != 'r'){
		fe_code.dsl = "cannot write inside zips";
		return 0;
	}
	index = zip_name_locate(lc->zip,name,ZIP_FL_NOCASE);
	if(index == -1){
		fe_code.dsl = "file not found in zip"; // fe_type is still FE_DSL from openDslFile
		return 0;
	}
	f->zfile = zip_fopen_index(lc->zip,index,0);
	if(!f->zfile){
		fe_type = FE_ZIPPED;
		fe_code.zipped = zip_get_error(lc->zip);
		return 0;
	}
	if(size){
		zip_stat_init(&stats);
		if(zip_stat_index(lc->zip,index,0,&stats) || ~stats.valid & ZIP_STAT_SIZE){
			zip_fclose(f->zfile);
			fe_code.dsl = "unknown file size";
			return 0;
		}
		*size = stats.size;
	}
	f->lc = lc;
	return 1;
	#else
	fe_code.dsl = "zip archive should not exist";
	return 0;
	#endif
}
static int openDslFileNormal(dsl_file *f,dsl_state *dsl,loader_collection *lc,const char *name,const char *mode,long *size){
	char *path;
	
	path = getDslFilePath(lc,name);
	if(!path){
		fe_code.dsl = "failed to allocate memory";
		return 0;
	}
	if(!isDslFileSafe(dsl,path)){
		free(path);
		fe_code.dsl = "forbidden file path";
		return 0;
	}
	f->file = fopen(path,mode);
	free(path);
	if(!f->file){
		fe_type = FE_STANDARD;
		fe_code.standard = errno;
		return 0;
	}
	if(size && (fseek(f->file,0,SEEK_END) || (*size = ftell(f->file)) == -1L || fseek(f->file,0,SEEK_SET))){
		fclose(f->file);
		fe_code.dsl = "unknown file size";
		return 0;
	}
	f->lc = NULL;
	return 1;
}
dsl_file* openDslFile(dsl_state *dsl,loader_collection *lc,const char *name,const char *mode,long *size){
	script_collection *sc;
	dsl_file *f;
	
	fe_type = FE_DSL;
	if(*mode != 'a' && *mode != 'r' && *mode != 'w')
		return fe_code.dsl = "invalid access mode",NULL;
	f = malloc(sizeof(dsl_file));
	if(!f)
		return fe_code.dsl = "failed to allocate memory",NULL;
	if(!lc)
		lc = (sc = dsl->manager->running_collection) ? sc->lc : NULL;
	if(lc && lc->zip){
		if(!openDslFileZipped(f,lc,name,mode,size)){
			free(f);
			return NULL;
		}
	}else if(!openDslFileNormal(f,dsl,lc,name,mode,size)){
		free(f);
		return NULL;
	}
	fe_type = FE_NONE;
	return f;
}
long readDslFile(dsl_file *f,char *buffer,long bytes){
	long read;
	
	#ifndef DSL_DISABLE_ZIP_FILES
	if(f->lc){
		read = zip_fread(f->zfile,buffer,bytes);
		if(read == -1)
			return 0;
		return read;
	}
	#endif
	return fread(buffer,1,bytes,f->file);
}
long writeDslFile(dsl_file *f,const char *buffer,long bytes){
	if(f->lc)
		return 0;
	return fwrite(buffer,1,bytes,f->file);
}
void closeDslFile(dsl_file *f){
	loader_collection *lc;
	char *buffer;
	
	#ifndef DSL_DISABLE_ZIP_FILES
	if(lc = f->lc)
		zip_fclose(f->zfile);
	else
	#endif
		fclose(f->file);
	free(f);
}
const char* getDslFileError(){
	switch(fe_type){
		case FE_STANDARD:
			return strerror(fe_code.standard);
		#ifndef DSL_DISABLE_ZIP_FILES
		case FE_ZIPPED:
			return zip_error_strerror(fe_code.zipped);
		#endif
		case FE_DSL:
			return fe_code.dsl;
	}
	return "no error";
}

// DEFAULT CONFIG
static int makeDefaultConfig(loader_collection *lc){
	char *buffer;
	FILE *file;
	
	buffer = getDslFilePath(lc,DSL_SCRIPT_CONFIG);
	if(!buffer)
		return 0;
	file = fopen(buffer,"wb");
	free(buffer);
	if(!file)
		return 0;
#ifdef DSL_SERVER_VERSION
	fprintf(file,"# derpy's script server: default script collection config\r\n\
\r\n\
\r\n\
# if this script collection should automatically start up when the game starts\r\n\
# only uncomment auto_start if it is necessary to override the preference set by the user\r\n\
\r\n\
#auto_start false\r\n\
\r\n\
\r\n\
# DSL version / settings required to run this script collection\r\n\
# only uncomment require_exact_version if you need an exact version\r\n\
\r\n\
require_version %d\r\n\
#require_exact_version %d\r\n\
#require_system_access true\r\n\
\r\n\
\r\n\
# main script(s) start when the collection starts (no main scripts are required for a server)\r\n\
# if a script ends with .lua and the file does not exist then .lur will be attempted afterwards\r\n\
\r\n\
#main_script main.lua\r\n\
\r\n\
\r\n\
# name of a script collection (or multiple on more lines) that must be started before this one\r\n\
# if a dependency is already running it will not be restarted but if missing then this collection will not start\r\n\
\r\n\
#require_dependency loadanim.lua\r\n\
\r\n\
\r\n\
# sometimes the name of a collection is very important so that it can be properly referenced by other mods\r\n\
# the name of the collection (determined by the file / folder name) must match require_name if it is uncommented\r\n\
\r\n\
#require_name example\r\n\
\r\n\
\r\n\
# client_script(s) are treated as the client's main_script(s) and client_file(s) are just extra files\r\n\
# you can uncomment and add as many client_script(s) or client_file(s) as you need to be sent to clients\r\n\
\r\n\
#client_script client.lua\r\n\
#client_file additional.lua",DSL_VERSION,DSL_VERSION);
#else
	fprintf(file,"# derpy's script loader: default script collection config\r\n\
\r\n\
\r\n\
# if this script collection should automatically start up when the game starts\r\n\
# only uncomment auto_start if it is necessary to override the preference set by the user\r\n\
\r\n\
#auto_start false\r\n\
\r\n\
\r\n\
# DSL version / settings required to run this script collection\r\n\
# only uncomment require_exact_version if you need an exact version\r\n\
\r\n\
require_version %d\r\n\
#require_exact_version %d\r\n\
#require_system_access true\r\n\
\r\n\
\r\n\
# main script(s) start when the collection starts (uses main.lua if none are specified)\r\n\
# if a script ends with .lua and the file does not exist then .lur will be attempted afterwards\r\n\
\r\n\
main_script main.lua\r\n\
\r\n\
\r\n\
# optional initialization script(s) can run during the pause menu before the game starts\r\n\
# these scripts will only have access to DSL functions and game threads (such as main) will not run\r\n\
\r\n\
#init_script init.lua\r\n\
\r\n\
\r\n\
# optional pre-initialization script(s) can run very early during the game's initialization\r\n\
# these scripts will only have access to DSL functions and cannot render or have any of its threads run\r\n\
\r\n\
#pre_init_script pre_init.lua\r\n\
\r\n\
\r\n\
# it is possible to make a collection that is only meant to register game files but have no scripts\r\n\
# uncomment disable_scripts to disable normal script loading and suppress the error for having no main script\r\n\
\r\n\
#disable_scripts true\r\n\
\r\n\
\r\n\
# name of a script collection (or multiple on more lines) that must be started before this one\r\n\
# if a dependency is already running it will not be restarted but if missing then this collection will not start\r\n\
\r\n\
#require_dependency loadanim.lua\r\n\
\r\n\
\r\n\
# sometimes the name of a collection is very important so that it can be properly referenced by other mods\r\n\
# the name of the collection (determined by the file / folder name) must match require_name if it is uncommented\r\n\
\r\n\
#require_name example\r\n\
\r\n\
\r\n\
# if this collection is meant to run while connected to servers uncomment this (no effect on server scripts)\r\n\
# it will keep the collection from being stopped when connecting to a server and allow control while connected\r\n\
# this is primarily meant for collections that don't affect gameplay as most servers would see that as cheating\r\n\
\r\n\
#allow_on_server true\r\n\
\r\n\
\r\n\
# register game files that should be put in a temporary IMG file (no effect on server scripts)\r\n\
# files are only registered once when the game starts regardless of if the collection is started or not\r\n\
# this whole system can disabled by the user so make sure to check if the IMG was replaced in your script if needed\r\n\
# the following fields can be uncommented to add or replace files in related IMG arhcives and you can list as many as desired\r\n\
\r\n\
#act_file file.cat\r\n\
#cuts_file file.dat\r\n\
#trigger_file file.dat\r\n\
#ide_file file.idb\r\n\
#scripts_file file.lur\r\n\
#world_file file.nft",DSL_VERSION,DSL_VERSION);
#endif
	fclose(file);
	return 1;
}

// COLLECTION REFRESH
static int loadCollectionZip(script_loader *sl,loader_collection *lc){
	char *buffer;
	char *name;
	size_t index;
	char backup;
	
	#ifndef DSL_DISABLE_ZIP_FILES
	buffer = malloc(strlen(lc->prefix) + strlen((char*)(lc+1)) + 1);
	if(!buffer){
		printConsoleError(sl->dsl->console,TEXT_FAIL_ALLOCATE,"collection zip path");
		return 0;
	}
	strcpy(buffer,DSL_SCRIPTS_PATH);
	strcat(buffer,(char*)(lc+1));
	lc->zip = zip_open(buffer,0,NULL);
	free(buffer);
	if(lc->zip){
		name = (char*)(lc+1);
		index = strlen(name) - 4; // the '.' in ".zip"
		backup = name[index+1];
		name[index] = '/';
		name[index+1] = 0;
		if(zip_name_locate(lc->zip,name,ZIP_FL_NOCASE) != -1)
			lc->flags |= LOADER_ZIPWARNING;
		else
			lc->flags &= ~LOADER_ZIPWARNING;
		name[index] = '.';
		name[index+1] = 'z';
		return 1;
	}
	printConsoleError(sl->dsl->console,"[%s] failed to open zip archive",(char*)(lc+1));
	#else
	printConsoleWarning(sl->dsl->console,"[%s] zip files are not supported in this version",(char*)(lc+1));
	#endif
	return 0;
}
static int loadCollectionConfig(script_loader *sl,loader_collection *lc){
	dsl_state *dsl;
	int reason;
	
	dsl = sl->dsl;
	lc->cfg = loadConfigSettingsEx(dsl,lc,DSL_SCRIPT_CONFIG,&reason);
	if(!lc->cfg && reason == CONFIG_MISSING){
		if(lc->zip)
			return 1;
		if(makeDefaultConfig(lc))
			printConsoleOutput(dsl->console,"[%s] generated default config",(char*)(lc+1));
		else
			printConsoleWarning(dsl->console,"[%s] failed to generate default config",(char*)(lc+1));
		lc->cfg = loadConfigSettingsEx(dsl,lc,DSL_SCRIPT_CONFIG,&reason);
	}
	if(lc->cfg || reason == CONFIG_EMPTY)
		return 1;
	if(reason == CONFIG_FAILURE)
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"collection config data");
	else if(reason == CONFIG_MISSING)
		printConsoleError(dsl->console,"[%s] failed to load config",(char*)(lc+1));
	return 0;
}
static void clearCollection(script_loader *sl,loader_collection *lc){
	lc->flags &= ~LOADER_EXISTS;
	if(lc->cfg){
		freeConfigSettings(lc->cfg);
		lc->cfg = NULL;
	}
}
static void refreshCollection(script_loader *sl,loader_collection *lc,int zip,int folder){
	if(sl){
		if(folder)
			lc->flags |= LOADER_FOLDER;
		else
			lc->flags &= ~LOADER_FOLDER;
		if(zip && !lc->zip && !loadCollectionZip(sl,lc))
			return;
		if((zip || folder) && !loadCollectionConfig(sl,lc))
			return;
	}
	if(!lc->sc){ // ONLY UPDATE THESE FLAGS IF NOT ALREADY RUNNING
		#ifdef DSL_SERVER_VERSION
		if(getConfigValue(lc->cfg,"client_script") || getConfigValue(lc->cfg,"client_file"))
			lc->flags |= LOADER_CLIENTFILES;
		else
			lc->flags &= ~LOADER_CLIENTFILES;
		#else
		if(~lc->flags & LOADER_NETWORK && getConfigBoolean(lc->cfg,"disable_scripts"))
			lc->flags |= LOADER_SCRIPTLESS;
		else
			lc->flags &= ~LOADER_SCRIPTLESS;
		if(~lc->flags & LOADER_NETWORK && getConfigBoolean(lc->cfg,"allow_on_server"))
			lc->flags |= LOADER_ALLOWSERVER;
		else
			lc->flags &= ~LOADER_ALLOWSERVER;
		#endif
	}
	lc->flags |= LOADER_EXISTS;
}

// COLLECTION REFRESHER
static int isLuaScript(const char *path){
	const char *ext;
	
	ext = NULL;
	while(*path)
		if(*(path++) == '.')
			ext = path;
	return ext && (!dslstrcmp(ext,"lua") || !dslstrcmp(ext,"lur"));
}
static int isZipArchive(const char *path){
	const char *ext;
	
	ext = NULL;
	while(*path)
		if(*(path++) == '.')
			ext = path;
	return ext && !dslstrcmp(ext,"zip");
}
static loader_collection* getCollection(script_loader *sl,const char *name,int idflag,const char *prefix){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next)
		if((lc->flags & idflag) == idflag && !dslstrcmp((char*)(lc+1),name)){
			if(strcmp(lc->prefix,prefix)){
				if(lc->sc || lc->flags & LOADER_EXISTS){
					printConsoleError(sl->dsl->console,"cannot have two script collections of the same name (%s%s, %s%s)",lc->prefix,(char*)(lc+1),prefix,name);
					return NULL;
				}else{
					printConsoleOutput(sl->dsl->console,"[%s] moved to %s",name,prefix);
					strcpy(lc->prefix,prefix); // update the prefix and return since it probably just moved
					strcpy((char*)(lc+1),name);
				}
			}
			return lc;
		}
	lc = malloc(sizeof(loader_collection) + strlen(name) + 1);
	if(!lc){
		printConsoleError(sl->dsl->console,TEXT_FAIL_ALLOCATE,"collection object");
		return NULL;
	}
	lc->next = sl->first;
	sl->first = lc;
	lc->sc = NULL;
	lc->cfg = NULL;
	lc->sc_cfg = NULL;
	lc->sc_cfg_ref = LUA_NOREF;
	lc->zip = NULL;
	lc->flags = idflag;
	strcpy(lc->prefix,prefix);
	strcpy((char*)(lc+1),name);
	return lc;
}
static void sortCollections(script_loader *sl){
	loader_collection *first,*nextadd,*adding,*where,*after;
	
	first = sl->first;
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
	sl->first = first;
}
#ifdef _WIN32
static void refreshFolder(script_loader *sl,char *buffer){
	loader_collection *lc;
	WIN32_FIND_DATA data;
	HANDLE search;
	size_t blen;
	int folder;
	int zip;
	
	search = FindFirstFile(buffer,&data);
	blen = strlen(buffer);
	buffer[--blen] = 0; // get rid of * part
	if(search != INVALID_HANDLE_VALUE){
		do{
			folder = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && strcmp(data.cFileName,".") && strcmp(data.cFileName,"..");
			if(!folder || *data.cFileName != '[' || data.cFileName[strlen(data.cFileName)-1] != ']'){
				zip = !folder && isZipArchive(data.cFileName);
				if((folder || zip || isLuaScript(data.cFileName)) && (lc = getCollection(sl,data.cFileName,LOADER_LOCAL,buffer)))
					refreshCollection(sl,lc,zip,folder);
			}else if(blen + strlen(data.cFileName) + 2 < MAX_PATH){ // +2 for "/*"
				strcpy(buffer+blen,data.cFileName);
				strcat(buffer,"/*");
				refreshFolder(sl,buffer);
				buffer[blen] = 0;
			}else
				printConsoleWarning(sl->dsl->console,"skipping %s%s because the path is too long",buffer,data.cFileName);
		}while(FindNextFile(search,&data));
		// should show error if FindNextFile failed?
		FindClose(search);
	}else if(GetLastError() != ERROR_FILE_NOT_FOUND)
		printConsoleError(sl->dsl->console,"failed to find script collections (path: %s, win32: %d)",buffer,GetLastError());
}
#else
static void refreshFolder(script_loader *sl,char *buffer){
	loader_collection *lc;
	struct dirent *dp;
	DIR *dir;
	size_t blen;
	int folder;
	int zip;
	
	dir = opendir(buffer);
	if(!dir){
		printConsoleError(sl->dsl->console,"failed to open script collections (path: %s, reason: %s)",buffer,strerror(errno));
		return;
	}
	blen = strlen(buffer);
	while(errno = 0,dp = readdir(dir)){
		folder = dp->d_type == DT_DIR && strcmp(dp->d_name,".") && strcmp(dp->d_name,"..");
		if(!folder || *dp->d_name != '[' || dp->d_name[strlen(dp->d_name)-1] != ']'){
			zip = !folder && isZipArchive(dp->d_name);
			if((folder || zip || isLuaScript(dp->d_name)) && (lc = getCollection(sl,dp->d_name,LOADER_LOCAL,buffer)))
				refreshCollection(sl,lc,zip,folder);
		}else if(blen + strlen(dp->d_name) + 1 < PATH_MAX){ // +1 for "/"
			strcpy(buffer+blen,dp->d_name);
			strcat(buffer,"/");
			refreshFolder(sl,buffer);
			buffer[blen] = 0;
		}else
			printConsoleWarning(sl->dsl->console,"skipping %s%s because the path is too long",buffer,dp->d_name);
	}
	if(errno)
		printConsoleError(sl->dsl->console,"failed to read script collections (path: %s, reason: %s)",buffer,strerror(errno));
	closedir(dir);
}
#endif
static void refreshCollections(script_loader *sl){
	loader_collection *lc;
	#ifdef _WIN32
	char buffer[MAX_PATH];
	#else
	char buffer[PATH_MAX];
	#endif
	
	for(lc = sl->first;lc;lc = lc->next)
		if(~lc->flags & LOADER_NETWORK)
			clearCollection(sl,lc);
	checkDslPathExists(DSL_SCRIPTS_PATH,0);
	strcpy(buffer,DSL_SCRIPTS_PATH);
	#ifdef _WIN32
	strcat(buffer,"*");
	#endif
	refreshFolder(sl,buffer);
	sortCollections(sl);
}

// COLLECTION UTILITY
static loader_collection* getCollectionByName(script_loader *sl,const char *name){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next)
		if(!dslstrcmp((char*)(lc+1),name))
			return lc;
	return NULL;
}
static const char* getCollectionStatus(loader_collection *lc){
	int flags;
	
	flags = lc->flags;
	if(lc->sc){
		if(flags & LOADER_RESTARTING)
			return "RESTARTING";
		if(flags & LOADER_STOPPING)
			return "STOPPING";
		return "RUNNING";
	}
	if(~flags & LOADER_EXISTS)
		return flags & LOADER_NETWORK ? "INACTIVE" : "MISSING";
	if(flags & LOADER_SCRIPTLESS)
		return "LOADED";
	if(flags & LOADER_FAILED && ~flags & LOADER_DIDNTAUTOSTART)
		return "FAILED";
	if(flags & LOADER_DUPLICATE)
		return "DUPLICATE";
	return "INACTIVE";
}
static const char* getCollectionType(loader_collection *lc){
	if(lc->flags & LOADER_NETWORK)
		return "NETWORK";
	if(lc->zip)
		return "ZIPPED";
	if(lc->flags & LOADER_FOLDER)
		return "NORMAL";
	return "STANDALONE";
}

// COLLECTION START
static int startCollection(script_loader *sl,loader_collection *lc,int initial,int netcheck);
static int issueCollectionWarnings(script_loader *sl,loader_collection *lc,script_console *console){
	loader_collection *cff;
	size_t index;
	
	if(lc->zip){ // check for folder
		index = strlen((char*)(lc+1)) - 4; // the '.' in ".zip"
		((char*)(lc+1))[index] = 0;
		for(cff = sl->first;cff;cff = cff->next)
			if(cff->flags & LOADER_FOLDER && cff->flags & LOADER_EXISTS && !dslstrcmp((char*)(lc+1),(char*)(cff+1)))
				break;
		((char*)(lc+1))[index] = '.';
		if(cff){
			lc->flags |= LOADER_DUPLICATE;
			return 1; // don't auto-start this script
		}
	}
	return 0;
}
static int startCollectionDependency(script_loader *sl,loader_collection *lc,const char *dep){
	loader_collection *dc;
	
	dc = getCollectionByName(sl,dep);
	if(!dc){
		printConsoleError(sl->dsl->console,"[%s] missing dependency \"%s\"",(char*)(lc+1),dep);
		return 0;
	}
	if(dc->flags & LOADER_STOPPING){
		printConsoleError(sl->dsl->console,"[%s] dependency \"%s\" is stopping",(char*)(lc+1),dep);
		return 0;
	}
	if(!dc->sc && ~dc->flags & LOADER_STARTING && !startCollection(sl,dc,0,1) && dc->flags & LOADER_FAILED){
		printConsoleError(sl->dsl->console,"[%s] failed to start dependency \"%s\"",(char*)(lc+1),dep);
		return 0;
	}
	return 1;
}
static int checkCollectionRequirements(script_loader *sl,loader_collection *lc){
	char buffer[MAX_LOADER_PATH];
	config_file *cfg;
	const char *dep;
	dsl_state *dsl;
	int x;
	
	cfg = lc->cfg;
	dsl = sl->dsl;
	if(getConfigValue(cfg,"require_exact_version") && (x = getConfigInteger(cfg,"require_exact_version")) != DSL_VERSION){
		printConsoleError(dsl->console,"[%s] DSL%d is required (running DSL%d, script collection not backwards compatible)",(char*)(lc+1),x,DSL_VERSION);
		return 0;
	}
	if((x = getConfigInteger(cfg,"require_version")) > DSL_VERSION){
		printConsoleError(dsl->console,"[%s] DSL%d is required (running DSL%d)",(char*)(lc+1),x,DSL_VERSION);
		return 0;
	}
	if(getConfigBoolean(cfg,"require_system_access") && ~dsl->flags & DSL_SYSTEM_ACCESS){
		printConsoleError(dsl->console,"[%s] system access is required (see DSL config)",(char*)(lc+1));
		return 0;
	}
	if((dep = getConfigString(cfg,"require_name")) && dslstrcmp((char*)(lc+1),dep)){
		printConsoleError(dsl->console,"[%s] name was expected to be %s (likely installed improperly)",(char*)(lc+1),dep);
		return 0;
	}
	if(lc->flags & LOADER_NETWORK)
		return 1; // client collections that came from a server can't have dependencies
	lc->flags |= LOADER_STARTING;
	for(x = 0;dep = getConfigStringArray(cfg,"require_dependency",x);x++){
		if(strlen(dep) >= sizeof(buffer)){
			printConsoleError(dsl->console,"[%s] the name of dependency #%d is too big (%zu / %zu bytes)",(char*)(lc+1),x+1,strlen(dep),sizeof(buffer)-1);
			lc->flags &= ~LOADER_STARTING;
			return 0;
		}
		if(!startCollectionDependency(sl,lc,strcpy(buffer,dep))){
			lc->flags &= ~LOADER_STARTING;
			return 0;
		}
	}
	lc->flags &= ~LOADER_STARTING;
	return 1;
}
static dsl_file* openCollectionMainScript(dsl_state *dsl,loader_collection *lc,char *fname,size_t bytes,int array){
	dsl_file *f;
	int lastindex;
	char backup;
	const char *name;
	
	#ifdef DSL_SERVER_VERSION
	name = getConfigStringArray(lc->cfg,"main_script",array);
	#else
	if(lc->flags & LOADER_NETWORK)
		name = getConfigStringArray(lc->cfg,"client_script",array);
	else
		name = getConfigStringArray(lc->cfg,!dsl->render ? "pre_init_script" : dsl->game ? "main_script" : "init_script",array);
	#endif
	if(name){
		if(strlen(name) >= bytes){
			printConsoleError(dsl->console,"[%s] main script name is too big (%zu / %zu bytes)",strlen(name),bytes-1);
			return NULL;
		}
		strcpy(fname,name);
	}else if(array
	#ifndef DSL_SERVER_VERSION
	|| !dsl->game || lc->flags & LOADER_NETWORK || getConfigValue(lc->cfg,"init_script") || getConfigValue(lc->cfg,"pre_init_script") // don't fail for no main script if there's other scripts
	#endif
	){ // not the first main script or not the main loader stage
		lc->flags &= ~LOADER_FAILED;
		return NULL; // don't start collection but don't mark it as a failure
	}else if(lc->zip || lc->flags & LOADER_FOLDER)
		strcpy(fname,"main.lua");
	else
		strcpy(fname,(char*)(lc+1));
	if(f = openDslFile(dsl,lc,fname,"rb",NULL))
		return f;
	if(isLuaScript(fname)){
		lastindex = strlen(fname) - 1;
		backup = fname[lastindex];
		fname[lastindex] = 'r';
		if(f = openDslFile(dsl,lc,fname,"rb",NULL))
			return f;
		fname[lastindex] = backup;
	}
	if(lc->flags & LOADER_ZIPWARNING)
		printConsoleWarning(dsl->console,"[%s] a folder with the same name as the zip exists in the root of the zip, please ensure that your files start in the root of the zip",(char*)(lc+1));
	printConsoleError(dsl->console,"[%s] failed to open main script (%s: %s)",(char*)(lc+1),getDslFileError(),fname);
	return NULL;
}
static script_collection* startCollectionInManager(dsl_state *dsl,loader_collection *lc){
	script_collection *sc;
	
	sc = createScriptCollection(dsl->manager,(char*)(lc+1),lc->prefix,lc);
	if(!sc)
		printConsoleError(dsl->console,"[%s] failed to create script collection",(char*)(lc+1));
	return sc;
}
static int startCollectionMainScript(dsl_state *dsl,loader_collection *lc,dsl_file *f,const char *fname,int initial){
	lua_State *lua;
	int dwr;
	
	lua = dsl->lua;
	if(createScript(lc->sc,initial,0,f,fname,lua,&dwr))
		return 1;
	if(!dwr){
		if(lua_isnil(lua,-1)){
			/*if(lc->zip || lc->flags & LOADER_FOLDER){
				lua_pop(lua,1);
				return 1; // collection stays alive because this is not a standalone collection
			}*/
			if(initial)
				lc->flags |= LOADER_DIDNTAUTOSTART; // also means it wont show FAILED for version require functions on init, but that's not a big deal
		}else if(lua_isstring(lua,-1))
			printConsoleError(dsl->console,"[%s] %s",(char*)(lc+1),lua_tostring(lua,-1));
		else
			printConsoleError(dsl->console,"[%s] an unknown error has occurred while running %s",(char*)(lc+1),fname);
		lua_pop(lua,1);
		return 0;
	}
	lua_pop(lua,1);
	return 1;
}
#ifdef DSL_SERVER_VERSION
static int copyClientFile(dsl_state *dsl,loader_collection *lc,const char *name){
	dsl_file *src;
	FILE *dest;
	size_t size;
	size_t bytes;
	union{
		char name[sizeof(DSL_CLIENT_SCRIPTS_PATH)+MAX_LOADER_PATH-1];
		char content[BUFSIZ];
	}buffer;
	
	if(strlen((char*)(lc+1)) + 1 + strlen(name) >= MAX_LOADER_PATH){
		printConsoleError(dsl->console,"[%s] client file path is too big (%zu / %zu bytes)",(char*)(lc+1),strlen(name),MAX_LOADER_PATH-2-strlen((char*)(lc+1)));
		return 0;
	}
	if(!checkNetworkExtension(name)){
		printConsoleError(dsl->console,"[%s] client file type is not allowed (%s)",(char*)(lc+1),name);
		return 0;
	}
	src = openDslFile(dsl,lc,name,"rb",&size);
	if(!src){
		printConsoleError(dsl->console,"[%s] failed to open client file for caching (%s: %s)",(char*)(lc+1),getDslFileError(),name);
		return 0;
	}
	strcpy(buffer.name,DSL_CLIENT_SCRIPTS_PATH);
	strcat(buffer.name,(char*)(lc+1));
	strcat(buffer.name,"/");
	strcat(buffer.name,name);
	if(!checkDslPathExists(buffer.name,1)){
		printConsoleError(dsl->console,"[%s] failed to create client file cache \"%s\"",(char*)(lc+1),buffer.name);
		return 0;
	}
	dest = fopen(buffer.name,"wb");
	if(!dest){
		printConsoleError(dsl->console,"[%s] failed to create client file cache (%s: %s)",(char*)(lc+1),strerror(errno),buffer.name);
		closeDslFile(src);
		return 0;
	}
	while(size){
		bytes = size < BUFSIZ ? size : BUFSIZ;
		if(readDslFile(src,buffer.content,bytes) != bytes || !fwrite(buffer.content,bytes,1,dest)){
			printConsoleError(dsl->console,"[%s] failed to cache client file \"%s\"",(char*)(lc+1),name);
			closeDslFile(src);
			fclose(dest);
			return 0;
		}
		size -= bytes;
	}
	closeDslFile(src);
	fclose(dest);
	return 1;
}
static int copyClientFiles(dsl_state *dsl,loader_collection *lc){
	const char *name;
	int array;
	
	for(array = 0;name = getConfigStringArray(lc->cfg,"client_script",array);array++)
		if(!copyClientFile(dsl,lc,name))
			return 0;
	for(array = 0;name = getConfigStringArray(lc->cfg,"client_file",array);array++)
		if(!copyClientFile(dsl,lc,name))
			return 0;
	copyClientFile(dsl,lc,DSL_SCRIPT_CONFIG);
	memset(lc->players,0xFF,sizeof(lc->players));
	return 1;
}
#endif
static int startCollection(script_loader *sl,loader_collection *lc,int initial,int netcheck){ // only call when sc == NULL
	dsl_state *dsl;
	dsl_file *f;
	char fname[MAX_LOADER_PATH];
	int array;
	
	#ifdef DEBUG_SAFETY_CHECKS
	if(lc->sc){
		printConsoleError(sl->dsl->console,"[%s] attempted to start while already started (please report)",(char*)(lc+1));
		return 0;
	}
	#endif
	if(lc->flags & LOADER_EXISTS)
		refreshCollection(NULL,lc,0,0);
	if(lc->flags & LOADER_SCRIPTLESS){
		lc->flags &= ~LOADER_FAILED; // this way it can still be required as a dependency
		return 0;
	}
	dsl = sl->dsl;
	#ifndef DSL_SERVER_VERSION
	if(netcheck && dsl->network && isNetworkPlayingOnServer(dsl->network)){
		printConsoleError(dsl->console,"[%s] you cannot start this script collection while on a server",(char*)(lc+1));
		return 0;
	}
	#endif
	lc->flags &= ~(LOADER_DUPLICATE | LOADER_DIDNTAUTOSTART); // these can't be the reasons for failure unless turned back on
	lc->flags |= LOADER_FAILED;
	#ifdef DSL_SERVER_VERSION
	if(areNetworkFilesTransfering(dsl->network)){
		printConsoleError(dsl->console,"[%s] cannot start because a player is downloading files (try again later)",(char*)(lc+1));
		return 0;
	}
	#endif
	if(~lc->flags & LOADER_EXISTS){
		printConsoleError(dsl->console,"[%s] the files for this script collection are missing",(char*)(lc+1));
		return 0;
	}
	if(issueCollectionWarnings(sl,lc,dsl->console) && initial){ // a warning will only stop the script from starting if auto-start
		lc->flags &= ~LOADER_FAILED; // not a real failure so dont show FAILED, and this won't affect dependency requirements because initial check
		return 0;
	}
	if(!checkCollectionRequirements(sl,lc))
		return 0;
	#ifdef DSL_SERVER_VERSION
	if((lc->zip || lc->flags & LOADER_FOLDER) && !getConfigValue(lc->cfg,"main_script")) // don't require non-standalone server collection main scripts
		f = NULL;
	else if(!(f = openCollectionMainScript(dsl,lc,fname,sizeof(fname),0)))
		return 0;
	#else
	f = openCollectionMainScript(dsl,lc,fname,sizeof(fname),0);
	if(!f)
		return 0;
	#endif
	lc->sc = startCollectionInManager(dsl,lc);
	if(!lc->sc){
		if(f)
			closeDslFile(f);
		return 0;
	}
	if(lc->cfg){
		lc->sc_cfg = copyConfigSettings(lc->cfg);
		if(!lc->sc_cfg){
			printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"collection config copy");
			lc->flags |= LOADER_STOPPING;
			shutdownScriptCollection(lc->sc,dsl->lua,1);
			if(f)
				closeDslFile(f);
			return 0;
		}
	}
	if(f){
		if(!startCollectionMainScript(dsl,lc,f,fname,initial)){
			if(lc->sc){ // sc could be NULL now if the script killed its own collection
				lc->flags |= LOADER_STOPPING;
				shutdownScriptCollection(lc->sc,dsl->lua,1);
			}
			if(f)
				closeDslFile(f);
			return 0;
		}
		closeDslFile(f);
	}
	for(array = 1;f = openCollectionMainScript(dsl,lc,fname,sizeof(fname),array);array++){ // opening will mark as not failed if there's no more scripts
		if(!startCollectionMainScript(dsl,lc,f,fname,initial)){
			if(lc->sc){
				lc->flags |= LOADER_STOPPING;
				shutdownScriptCollection(lc->sc,dsl->lua,1);
			}
			closeDslFile(f);
			return 0; // may or may not be a failure (might be a version requirement or a dont-auto-start), but a collection can only start if all main scripts can start
		}
		closeDslFile(f);
	}
	#ifdef DSL_SERVER_VERSION
	if(lc->flags & LOADER_CLIENTFILES && !copyClientFiles(dsl,lc)){
		lc->flags |= LOADER_STOPPING;
		shutdownScriptCollection(lc->sc,dsl->lua,1);
		return 0;
	}
	updateNetworkPlayerFiles(dsl);
	#endif
	return 1;
}

// COLLECTION STOP
static void stopCollection(script_loader *sl,loader_collection *lc){ // only call when sc != NULL and not LOADER_STOPPING
	#ifdef DEBUG_SAFETY_CHECKS
	if(!lc->sc || lc->flags & LOADER_STOPPING){
		printConsoleError(sl->dsl->console,"[%s] attempted to stop while already stopped (please report)",(char*)(lc+1));
		return;
	}
	#endif
	lc->flags |= LOADER_STOPPING;
	#ifdef DSL_SERVER_VERSION
	updateNetworkPlayerFiles(sl->dsl);
	#endif
	shutdownScriptCollection(lc->sc,sl->dsl->lua,1);
}

// COMMAND UTILITY
static int canCommandCollection(script_loader *sl,loader_collection *lc,int starting){
	#ifndef DSL_SERVER_VERSION
	dsl_state *dsl;
	
	dsl = sl->dsl;
	if(!dsl->network || !isNetworkPlayingOnServer(dsl->network))
		return 1;
	if(starting || lc->flags & LOADER_NETWORK)
		return 0;
	#endif
	return 1;
}
static loader_collection* getCollectionFromArgv(script_loader *sl,int argc,char **argv,int idflag){
	loader_collection *lc;
	
	if(!argc){
		printConsoleError(sl->dsl->console,"expected script collection name");
		return NULL;
	}
	for(lc = sl->first;lc;lc = lc->next)
		if((lc->flags & idflag) == idflag && !dslstrcmp((char*)(lc+1),*argv))
			return lc;
	if(idflag == LOADER_LOCAL)
		return getCollectionFromArgv(sl,argc,argv,LOADER_NETWORK);
	printConsoleError(sl->dsl->console,"unknown script collection \"%s\"",*argv);
	return NULL;
}
static void printCollectionCheck(loader_collection *lc,script_console *console){
	script *s;
	int sc;
	int tc;
	
	if(lc->sc){
		tc = sc = 0;
		for(s = lc->sc->scripts;s;s = s->next){
			sc++;
			tc += s->thread_count;
		}
		printConsoleOutput(console,"[%s] type: %s, status: %s, scripts: %d, threads: %d, prefix: %s",(char*)(lc+1),getCollectionType(lc),getCollectionStatus(lc),sc,tc,getScriptCollectionPrefix(lc->sc));
	}else
		printConsoleOutput(console,"[%s] type: %s, status: %s, prefix: %s",(char*)(lc+1),getCollectionType(lc),getCollectionStatus(lc),lc->prefix);
}

// COMMAND FUNCTIONS
static void startCommand(script_loader *sl,int argc,char **argv){
	loader_collection *lc;
	dsl_state *dsl;
	
	refreshCollections(sl);
	if(argc && **argv == '*'){
		dsl = sl->dsl;
		#ifndef DSL_SERVER_VERSION
		if(dsl->network && isNetworkPlayingOnServer(dsl->network)){
			printConsoleError(dsl->console,"cannot start all collections while on a server");
			return;
		}
		#endif
		printConsoleOutput(dsl->console,"starting / restaring all script collections");
		for(lc = sl->first;lc;lc = lc->next)
			if(lc->sc){
				if(canCommandCollection(sl,lc,1)){
					lc->flags |= LOADER_RESTARTING;
					if(~lc->flags & LOADER_STOPPING)
						stopCollection(sl,lc);
				}
			}else if(lc->flags & LOADER_EXISTS && ~lc->flags & LOADER_SCRIPTLESS)
				startCollection(sl,lc,0,1);
	}else if(lc = getCollectionFromArgv(sl,argc,argv,LOADER_LOCAL)){
		if(lc->flags & LOADER_SCRIPTLESS)
			printConsoleError(sl->dsl->console,"[%s] no scripts to start",(char*)(lc+1));
		else if(!lc->sc){
			printConsoleOutput(sl->dsl->console,"starting %s",(char*)(lc+1));
			startCollection(sl,lc,0,1);
		}else if(canCommandCollection(sl,lc,1)){
			printConsoleOutput(sl->dsl->console,"restarting %s",(char*)(lc+1));
			lc->flags |= LOADER_RESTARTING;
			if(~lc->flags & LOADER_STOPPING)
				stopCollection(sl,lc);
		}else
			printConsoleError(sl->dsl->console,"[%s] you cannot restart this script collection while on a server",(char*)(lc+1));
	}
}
static void stopCommand(script_loader *sl,int argc,char **argv){
	loader_collection *lc;
	dsl_state *dsl;
	
	refreshCollections(sl);
	if(argc && **argv == '*'){
		dsl = sl->dsl;
		#ifndef DSL_SERVER_VERSION
		if(dsl->network && isNetworkPlayingOnServer(dsl->network))
			printConsoleOutput(dsl->console,"stopping all local script collections");
		else
		#endif
			printConsoleOutput(dsl->console,"stopping all script collections");
		for(lc = sl->first;lc;lc = lc->next)
			if(lc->sc && canCommandCollection(sl,lc,0)){
				lc->flags &= ~LOADER_RESTARTING;
				if(~lc->flags & LOADER_STOPPING)
					stopCollection(sl,lc);
			}
	}else if(lc = getCollectionFromArgv(sl,argc,argv,LOADER_LOCAL)){
		if(!canCommandCollection(sl,lc,0)){
			printConsoleError(sl->dsl->console,"[%s] you cannot stop this script collection while on a server",(char*)(lc+1));
		}else if(lc->sc){
			printConsoleOutput(sl->dsl->console,"stopping %s",(char*)(lc+1));
			lc->flags &= ~LOADER_RESTARTING;
			if(~lc->flags & LOADER_STOPPING)
				stopCollection(sl,lc);
		}else
			printConsoleError(sl->dsl->console,"%s is already stopped",(char*)(lc+1));
	}
}
static void restartCommand(script_loader *sl,int argc,char **argv){
	loader_collection *lc;
	dsl_state *dsl;
	
	refreshCollections(sl);
	if(argc && **argv == '*'){
		dsl = sl->dsl;
		#ifndef DSL_SERVER_VERSION
		if(dsl->network && isNetworkPlayingOnServer(dsl->network)){
			printConsoleError(dsl->console,"cannot restart all collections while on a server");
			return;
		}
		#endif
		printConsoleOutput(dsl->console,"restaring running script collections");
		for(lc = sl->first;lc;lc = lc->next)
			if(lc->sc && ~lc->flags & LOADER_STOPPING && ~lc->flags & LOADER_SCRIPTLESS && canCommandCollection(sl,lc,1)){
				lc->flags |= LOADER_RESTARTING;
				stopCollection(sl,lc);
			}
	}else if(lc = getCollectionFromArgv(sl,argc,argv,LOADER_LOCAL)){
		if(lc->flags & LOADER_SCRIPTLESS)
			printConsoleError(sl->dsl->console,"[%s] no scripts to start",(char*)(lc+1));
		else if(!lc->sc){
			printConsoleOutput(sl->dsl->console,"starting %s",(char*)(lc+1));
			startCollection(sl,lc,0,1);
		}else if(canCommandCollection(sl,lc,1)){
			printConsoleOutput(sl->dsl->console,"restarting %s",(char*)(lc+1));
			lc->flags |= LOADER_RESTARTING;
			if(~lc->flags & LOADER_STOPPING)
				stopCollection(sl,lc);
		}else
			printConsoleError(sl->dsl->console,"[%s] you cannot restart this script collection while on a server",(char*)(lc+1));
	}
}
static void listCommand(script_loader *sl,int argc,char **argv){
	loader_collection *lc;
	int count;
	
	refreshCollections(sl);
	if(argc)
		for(count = 0;(*argv)[count];count++)
			(*argv)[count] = toupper((*argv)[count]);
	count = 0;
	for(lc = sl->first;lc;lc = lc->next)
		if(!argc || !dslstrcmp(getCollectionStatus(lc),*argv))
			count++;
	printConsoleOutput(sl->dsl->console,"[%s SCRIPT COLLECTIONS: %d]",argc ? *argv : "TOTAL",count);
	for(lc = sl->first;lc;lc = lc->next)
		if(!argc || !dslstrcmp(getCollectionStatus(lc),*argv))
			if(lc->flags & LOADER_NETWORK)
				printConsoleSpecial(sl->dsl->console," [%s] %s",getCollectionStatus(lc),(char*)(lc+1));
			else
				printConsoleOutput(sl->dsl->console," [%s] %s",getCollectionStatus(lc),(char*)(lc+1));
}
static void checkCommand(script_loader *sl,int argc,char **argv){
	loader_collection *lc;
	
	refreshCollections(sl);
	if(argc && **argv == '*')
		for(lc = sl->first;lc;lc = lc->next)
			printCollectionCheck(lc,sl->dsl->console);
	else if(lc = getCollectionFromArgv(sl,argc,argv,LOADER_LOCAL))
		printCollectionCheck(lc,sl->dsl->console);
}

// DESTROYED CALLBACK
#ifndef DSL_SERVER_VERSION
static void checkForServerPrep(script_loader *sl,loader_collection *lc){
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->flags & LOADER_NETBLOCKER)
			return;
	preparedNetworkScripts(sl->dsl);
}
#endif
static int destroyedCollection(lua_State *lua,script_loader *sl,script_collection *sc){
	loader_collection *lc;
	config_file **ptr;
	
	if(lc = sc->lc){
		if(lc->sc_cfg){
			freeConfigSettings(lc->sc_cfg);
			lc->sc_cfg = NULL;
		}
		lc->sc = NULL;
		lc->flags &= ~LOADER_STOPPING;
		if(lc->flags & LOADER_RESTARTING){
			lc->flags &= ~LOADER_RESTARTING;
			startCollection(sl,lc,0,~lc->flags & LOADER_NETWORK);
		}
		#ifndef DSL_SERVER_VERSION
		if(lc->flags & LOADER_NETBLOCKER){
			lc->flags &= ~LOADER_NETBLOCKER;
			checkForServerPrep(sl,lc);
		}
		#endif
	}
	return 0;
}

// LOADER STATE
script_loader* createScriptLoader(dsl_state *dsl){
	script_loader *sl;
	
	sl = malloc(sizeof(script_loader));
	if(!sl)
		return NULL;
	sl->first = NULL;
	sl->dsl = dsl;
	setScriptCommandEx(dsl->cmdlist,"start",TEXT_HELP_START,&startCommand,sl,1);
	setScriptCommandEx(dsl->cmdlist,"stop",TEXT_HELP_STOP,&stopCommand,sl,1);
	setScriptCommandEx(dsl->cmdlist,"restart",TEXT_HELP_RESTART,&restartCommand,sl,1);
	setScriptCommandEx(dsl->cmdlist,"list",TEXT_HELP_LIST,&listCommand,sl,1);
	setScriptCommandEx(dsl->cmdlist,"check",TEXT_HELP_CHECK,&checkCommand,sl,1);
	addScriptEventCallback(dsl->events,EVENT_COLLECTION_DESTROYED,&destroyedCollection,sl);
	return sl;
}
void startScriptLoader(dsl_state *dsl,script_loader *sl){
	loader_collection *lc;
	int auto_start;
	int force_pref;
	
	refreshCollections(sl);
	#ifndef DSL_SERVER_VERSION
	if(dsl->content)
		addContentUsingLoader(dsl);
	#endif
	auto_start = !getConfigBoolean(dsl->config,"dont_auto_start");
	force_pref = getConfigBoolean(dsl->config,"force_auto_start_pref");
	for(lc = sl->first;lc;lc = lc->next)
		if(!lc->sc && (force_pref ? auto_start : (getConfigValue(lc->cfg,"auto_start") ? getConfigBoolean(lc->cfg,"auto_start") : auto_start)))
			startCollection(sl,lc,1,1);
}
void destroyScriptLoader(script_loader *sl){
	loader_collection *lc,*nlc;
	
	for(lc = sl->first;lc;lc = nlc){
		if(lc->sc_cfg)
			freeConfigSettings(lc->sc_cfg);
		#ifndef DSL_DISABLE_ZIP_FILES
		if(lc->zip)
			zip_discard(lc->zip);
		#endif
		clearCollection(sl,lc);
		nlc = lc->next;
		free(lc);
	}
	free(sl);
}

// LOADER API
int stopScriptLoaderCollection(script_loader *sl,script_collection *sc){
	loader_collection *lc;
	
	lc = sc->lc;
	if(!lc || lc->flags & LOADER_STOPPING)
		return 0;
	stopCollection(sl,lc);
	return 1;
}
int startScriptLoaderDependency(script_loader *sl,script_collection *sc,const char *dep){
	loader_collection *lc;
	
	lc = sc->lc;
	if(!lc){
		printConsoleError(sl->dsl->console,"[%s] cannot start dependency \"%s\"",getScriptCollectionName(sc),dep);
		return 0;
	}
	refreshCollections(sl);
	lc->flags |= LOADER_STARTING;
	if(!startCollectionDependency(sl,lc,dep)){
		lc->flags &= ~LOADER_STARTING;
		return 0;
	}
	lc->flags &= ~LOADER_STARTING;
	return 1;
}
#ifdef DSL_SERVER_VERSION
int getScriptLoaderPlayerBit(loader_collection *lc,unsigned id){
	return lc->players[id / (sizeof(int) * 8)] & 1 << id % (sizeof(int) * 8);
}
void setScriptLoaderPlayerBit(loader_collection *lc,unsigned id,int on){
	if(on)
		lc->players[id / (sizeof(int) * 8)] |= 1 << id % (sizeof(int) * 8);
	else
		lc->players[id / (sizeof(int) * 8)] &= ~(1 << id % (sizeof(int) * 8));
}
void setScriptLoaderPlayerBits(script_loader *sl,unsigned id,int on){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next)
		setScriptLoaderPlayerBit(lc,id,on);
}
#else
void prepareScriptLoaderForServer(script_loader *sl){
	loader_collection *lc;
	int none;
	
	// TODO: probably remove ~lc->flags & LOADER_NETWORK because old net scripts SHOULD still turn on LOADER_NETBLOCKER.
	
	none = 1;
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->sc && ~lc->flags & LOADER_NETWORK && ~lc->flags & LOADER_ALLOWSERVER){
			#ifdef NET_PRINT_DEBUG_MESSAGES
			printConsoleOutput(sl->dsl->console,"[%s] stopping for preparation of network play",(char*)(lc+1));
			#endif
			lc->flags |= LOADER_NETBLOCKER;
		}else
			lc->flags &= ~LOADER_NETBLOCKER;
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->flags & LOADER_NETBLOCKER){
			lc->flags &= ~LOADER_RESTARTING;
			if(~lc->flags & LOADER_STOPPING)
				stopCollection(sl,lc);
			none = 0;
		}
	if(none)
		preparedNetworkScripts(sl->dsl);
}
static int isInActiveScriptList(char *name,unsigned count,char **active){
	while(count)
		if(!dslstrcmp(name,active[--count]))
			return 1;
	return 0;
}
void refreshScriptLoaderServerScripts(script_loader *sl,unsigned count,char **active){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next) // clear all collections
		if(lc->flags & LOADER_NETWORK)
			clearCollection(sl,lc);
	while(count) // refresh active collections
		if(lc = getCollection(sl,active[--count],LOADER_NETWORK,DSL_SERVER_SCRIPTS_PATH))
			refreshCollection(sl,lc,0,1);
}
void ensureScriptLoaderServerScripts(script_loader *sl,unsigned count,char **active){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->flags & LOADER_NETWORK)
			if(isInActiveScriptList((char*)(lc+1),count,active)){
				if((!lc->sc || (lc->flags & LOADER_STOPPING && ~lc->flags & LOADER_RESTARTING)) && ~lc->flags & LOADER_NETRESTARTED){
					#ifdef NET_PRINT_DEBUG_MESSAGES
					printConsoleWarning(sl->dsl->console,"ensuring the starting of %s",(char*)(lc+1));
					#endif
					if(lc->sc)
						lc->flags |= LOADER_RESTARTING;
					else
						startCollection(sl,lc,0,0);
				}
			}else if(lc->sc){
				#ifdef NET_PRINT_DEBUG_MESSAGES
				if(lc->flags & (LOADER_STOPPING | LOADER_RESTARTING))
					printConsoleOutput(sl->dsl->console,"ensuring the stopping of %s",(char*)(lc+1));
				#endif
				lc->flags &= ~LOADER_RESTARTING;
				if(~lc->flags & LOADER_STOPPING)
					stopCollection(sl,lc);
			}
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->flags & LOADER_NETWORK)
			lc->flags &= ~LOADER_NETRESTARTED;
}
void restartScriptLoaderServerScript(script_loader *sl,unsigned count,char **active,const char *name){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->flags & LOADER_NETWORK && !dslstrcmp((char*)(lc+1),name) && isInActiveScriptList((char*)(lc+1),count,active)){
			if(lc->sc){
				#ifdef NET_PRINT_DEBUG_MESSAGES
				printConsoleSpecial(sl->dsl->console,"restarting %s",(char*)(lc+1));
				#endif
				lc->flags |= LOADER_RESTARTING;
				if(~lc->flags & LOADER_STOPPING)
					stopCollection(sl,lc);
			}else{
				#ifdef NET_PRINT_DEBUG_MESSAGES
				printConsoleSpecial(sl->dsl->console,"starting %s",(char*)(lc+1));
				#endif
				startCollection(sl,lc,0,0);
			}
			lc->flags |= LOADER_NETRESTARTED;
			return;
		}
}
void killScriptLoaderServerScripts(script_loader *sl,const char *only){
	loader_collection *lc;
	
	for(lc = sl->first;lc;lc = lc->next)
		if(lc->sc && lc->flags & LOADER_NETWORK && (!only || !dslstrcmp((char*)(lc+1),only))){
			#ifdef NET_PRINT_DEBUG_MESSAGES
			if(lc->flags & (LOADER_STOPPING | LOADER_RESTARTING))
				printConsoleSpecial(sl->dsl->console,"stopping %s",(char*)(lc+1));
			#endif
			lc->flags &= ~LOADER_RESTARTING;
			if(~lc->flags & LOADER_STOPPING)
				stopCollection(sl,lc);
		}
}
#endif
