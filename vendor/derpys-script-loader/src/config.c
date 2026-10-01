/* DERPY'S SCRIPT LOADER: CONFIG PARSER
	
	parses config files in a pretty basic way
	blank lines are ignored and # marks the rest of a line as a comment
	config options are specified in key value pairs with whitespace in between
	key names are case sensitive and value types are only defined by their usage
	the same key name can be given multiple times if desired for a list of values
	":" / "=" are allowed between keys and values and "[" on the start of a line ignores it
	
*/

#include <dsl/dsl.h>
#include <string.h>
#include <ctype.h>

#define isspace2(c) (isspace(c) || c == ':' || c == '=')

// Config struct.
struct config_file{
	unsigned count;
	char **keys;
	char **values;
	char *buffer;
	size_t strsiz;
	char *strbuf;
};

// Utility functions.
static char* loadConfigFile(const char *filename){
	FILE *file;
	char *config;
	long size;
	
	file = fopen(filename,"rb");
	if(!file)
		return NULL;
	if(fseek(file,0,SEEK_END) || (size = ftell(file)) == -1L || fseek(file,0,SEEK_SET)){
		fclose(file);
		return NULL;
	}
	config = malloc(size + 1); // the entire file stays loaded and strings are just cut using null terminators, so we need to make sure there's room at the end.
	if(!config){
		fclose(file);
		return NULL;
	}
	if(!fread(config,size,1,file)){
		fclose(file);
		free(config);
		return NULL;
	}
	fclose(file);
	config[size] = 0;
	return config;
}
static char* loadConfigFile2(dsl_state *dsl,loader_collection *lc,const char *filename){
	dsl_file *file;
	char *config;
	long size;
	
	file = openDslFile(dsl,lc,filename,"rb",&size);
	if(!file)
		return NULL;
	config = malloc(size + 1); // the entire file stays loaded and strings are just cut using null terminators, so we need to make sure there's room at the end.
	if(!config){
		closeDslFile(file);
		return NULL;
	}
	if(readDslFile(file,config,size) != size){
		closeDslFile(file);
		free(config);
		return NULL;
	}
	closeDslFile(file);
	config[size] = 0;
	return config;
}
static unsigned getConfigFields(char *config,char ***r_keys,char ***r_values,int *status){
	char c,*str;
	char *key,**keys,**values;
	char *vwspace; // white space after value
	unsigned count;
	int counted,comment;
	
	// Allocate enough fields (just line count is fine).
	count = 0;
	comment = counted = 0;
	for(str = config;c = *str;str++)
		if(c == '#' || c == '[')
			comment = 1;
		else if(c == '\r' || c == '\n')
			comment = counted = 0;
		else if(!counted && !comment && !isspace2(c)){
			counted = 1;
			count++;
		}
	if(!count){
		if(status)
			*status = CONFIG_EMPTY;
		return 0;
	}
	keys = malloc(count*sizeof(char*));
	if(!keys){
		if(status)
			*status = CONFIG_FAILURE;
		return 0;
	}
	values = malloc(count*sizeof(char*));
	if(!values){
		if(status)
			*status = CONFIG_FAILURE;
		free(keys);
		return 0;
	}
	
	// Parse fields.
	count = 0;
	comment = 0;
	for(str = config;c = *str;str++){
		if(c == '#' || c == '['){
			comment = 1;
		}else if(c == '\r' || c == '\n'){
			comment = 0;
		}else if(!comment && !isspace2(c)){
			key = str; // 1. read key
			while((c = *(++str)) && c != '#' && !isspace2(c))
				;
			if(c && c != '#' && c != '\r' && c != '\n'){
				*str = 0;
				while((c = *(++str)) && isspace2(c)) // 2. skip whitespace
					;
				if(c && c != '#' && c != '\r' && c != '\n'){
					keys[count] = key; // 3. save key/value and read value
					values[count++] = str;
					vwspace = NULL;
					while((c = *(++str)) && c != '#' && c != '\r' && c != '\n')
						if(vwspace){
							if(!isspace2(c))
								vwspace = NULL;
						}else if(isspace2(c))
							vwspace = str;
					if(vwspace)
						*vwspace = 0;
					else
						*str = 0;
				}
			}
			while(c && c != '\r' && c != '\n')
				c = *(++str); // keep reading until end of line
			if(!c)
				break; // hit end while reading key/value
		}
	}
	if(!count){
		if(status)
			*status = CONFIG_EMPTY;
		free(keys);
		free(values);
		return 0;
	}
	
	// Return.
	*r_keys = keys;
	*r_values = values;
	return count;
}

// String conversion.
static const char* convertString(config_file *ptr,const char *raw){
	size_t length;
	size_t i;
	
	if((*raw == '\'' || *raw == '"') && raw[1]){
		raw++;
		if((length = strlen(raw)) >= ptr->strsiz){
			if(ptr->strsiz)
				free(ptr->strbuf);
			ptr->strbuf = malloc(ptr->strsiz = length + 1);
			if(!ptr->strbuf){
				ptr->strsiz = 0;
				return NULL; // no error raised because it's too common of a function and too low of a chance of failure
			}
		}
		strcpy(ptr->strbuf,raw);
		for(i = 0;i < length;i++)
			if(raw[i] == '\'' || raw[i] == '"'){
				ptr->strbuf[i] = 0;
				return ptr->strbuf;
			}
		raw--;
	}
	return raw;
}

// Config functions.
config_file* loadConfigSettingsEx(dsl_state *dsl,loader_collection *lc,const char *filename,int *status){
	config_file *result;
	char *buffer;
	
	result = malloc(sizeof(config_file));
	if(!result){
		if(status)
			*status = CONFIG_FAILURE;
		return NULL;
	}
	if(dsl)
		buffer = loadConfigFile2(dsl,lc,filename);
	else
		buffer = loadConfigFile(filename);
	if(!buffer){
		if(status)
			*status = CONFIG_MISSING;
		free(result);
		return NULL;
	}
	result->count = getConfigFields(buffer,&result->keys,&result->values,status);
	if(!result->count){
		free(buffer);
		free(result);
		return NULL;
	}
	result->buffer = buffer;
	result->strsiz = 0;
	return result;
}
void freeConfigSettings(config_file *ptr){
	if(!ptr)
		return;
	if(ptr->strsiz)
		free(ptr->strbuf);
	free(ptr->buffer);
	free(ptr->values);
	free(ptr->keys);
	free(ptr);
}
int getConfigValueCount(config_file *ptr,const char *key){
	unsigned count;
	char **keys;
	int r;
	
	if(!ptr)
		return 0;
	r = 0;
	keys = ptr->keys;
	count = ptr->count;
	while(count)
		if(!strcmp(keys[--count],key))
			r++;
	return r;
}
const char* getConfigValueArray(config_file *ptr,const char *key,int index){
	unsigned count;
	unsigned iter;
	char **keys;
	
	if(!ptr)
		return NULL;
	keys = ptr->keys;
	count = ptr->count;
	for(iter = 0;iter < count;iter++)
		if(!strcmp(keys[iter],key))
			if(index)
				index--;
			else
				return ptr->values[iter];
	return NULL;
}
const char* getConfigStringArray(config_file *ptr,const char *key,int index){
	const char *value;
	
	if(value = getConfigValueArray(ptr,key,index))
		return convertString(ptr,value);
	return NULL;
}
int getConfigBooleanArray(config_file *ptr,const char *key,int index){
	const char *value;
	
	if(value = getConfigValueArray(ptr,key,index)){
		if(*value >= '0' && *value <= '9')
			return *value != '0';
		return tolower(*value) != 'f';
	}
	return 0;
}
int getConfigIntegerArray(config_file *ptr,const char *key,int index){
	const char *value;
	
	if(value = getConfigValueArray(ptr,key,index))
		return strtol(value,NULL,0);
	return 0;
}

// Copy config.
config_file* copyConfigSettings(config_file *ptr){
	config_file *copy;
	unsigned count;
	char *finish;
	size_t size;
	
	copy = malloc(sizeof(config_file));
	if(!copy)
		return NULL;
	finish = ptr->values[ptr->count-1];
	while(*(finish++)) // goes just past the null terminator of the final value
		;
	count = ptr->count;
	copy->keys = malloc(count*sizeof(char*));
	copy->values = malloc(count*sizeof(char*));
	copy->buffer = malloc(size = finish - ptr->buffer);
	copy->strsiz = 0;
	if(!copy->keys || !copy->values || !copy->buffer){
		if(copy->keys)
			free(copy->keys);
		if(copy->values)
			free(copy->values);
		if(copy->buffer)
			free(copy->buffer);
		free(copy);
		return NULL;
	}
	copy->count = count;
	while(count){
		count--;
		copy->keys[count] = copy->buffer + (ptr->keys[count] - ptr->buffer);
		copy->values[count] = copy->buffer + (ptr->values[count] - ptr->buffer);
	}
	memcpy(copy->buffer,ptr->buffer,size);
	return copy;
}