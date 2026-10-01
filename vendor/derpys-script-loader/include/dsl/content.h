/* DERPY'S SCRIPT LOADER: CONTENT LOADER
	
	scripts can specify new files for the game to use in its IMG archives
	if any files are used for a certain archive then a new one is created in cache to be used
	
*/

#ifndef DSL_CONTENT_H
#define DSL_CONTENT_H

struct dsl_state;

#define CONTENT_ACT_IMG 0
#define CONTENT_CUTS_IMG 1
#define CONTENT_TRIGGER_IMG 2
#define CONTENT_IDE_IMG 3
#define CONTENT_SCRIPTS_IMG 4
#define CONTENT_WORLD_IMG 5
#define CONTENT_TYPES 6

#ifndef DSL_SERVER_VERSION

typedef struct script_content script_content;

#ifdef __cplusplus
extern "C" {
#endif

script_content* createContentManager(struct dsl_state *dsl);
void destroyContentManager(script_content *sc);
void freeContentListings(script_content *sc);
int wasContentReplaced(script_content *sc,int type); // reset between dsl stages so kind of worthless

int addContentFile(script_content *sc,loader_collection *lc,int type,const char *path); // zero = failed, usually if the name is too long
void addContentUsingLoader(struct dsl_state *dsl);
int makeArchiveWithContent(struct dsl_state *dsl,int type,const char *spath,const char *dpath); // non-zero = an archive was made and should be used

int generateContentHashes(dsl_state *dsl);
int getContentHash(int type);

#ifdef __cplusplus
}
#endif

#endif

#endif