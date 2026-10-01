#include <stdlib.h>
#include <string.h>

void* dslmalloc(size_t bytes){
	void *result;
	
	if(result = malloc(bytes))
		memset(result,0xFF,bytes);
	return result;
}