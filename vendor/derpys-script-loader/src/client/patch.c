/* DERPY'S SCRIPT LOADER: PATCH UTILITY
	
	these are the functions used for patching the game's code
	code can be replaced with a jump/call to another function
	call replacements take an amount of bytes so that NOP can remove some code
	there's also a prefix version of call replaces for possibly pushing a register
	
*/

#include <dsl/dsl.h>
#include <string.h>

int nullifyInstructions(void *ptr,size_t bytes){
	DWORD reset;
	
	if(VirtualProtect(ptr,bytes,PAGE_READWRITE,&reset)){
		memset(ptr,0x90,bytes);
		return VirtualProtect(ptr,bytes,reset,&reset);
	}
	return 0;
}
int replaceImmediateValue(void *ptr,void *value){
	DWORD reset;
	
	if(VirtualProtect(ptr,4,PAGE_READWRITE,&reset)){
		*(void**)ptr = value;
		return VirtualProtect(ptr,4,reset,&reset);
	}
	return 0;
}
int replaceImmediateByte(void *ptr,char value){
	DWORD reset;
	
	if(VirtualProtect(ptr,1,PAGE_READWRITE,&reset)){
		*(char*)ptr = value;
		return VirtualProtect(ptr,1,reset,&reset);
	}
	return 0;
}
int replaceCodeWithJump(void *ptr,void *target){
	DWORD reset;
	
	if(VirtualProtect(ptr,5,PAGE_READWRITE,&reset)){
		*(unsigned char*)ptr = 0xE9;
		*(int32_t*)((char*)ptr+1) = (int32_t)target - ((int32_t)ptr+5);
		return VirtualProtect(ptr,5,reset,&reset);
	}
	return 0;
}
int replaceFunctionCall(void *ptr,size_t bytes,void *replacement){
	DWORD reset;
	
	if(VirtualProtect(ptr,bytes,PAGE_READWRITE,&reset)){
		*(unsigned char*)ptr = 0xE8;
		*(int32_t*)((char*)ptr+1) = (int32_t)replacement - ((int32_t)ptr+5);
		if(bytes > 5)
			memset((char*)ptr+5,0x90,bytes-5);
		return VirtualProtect(ptr,bytes,reset,&reset);
	}
	return 0;
}
int replaceFunctionCallWithPrefix(void *ptr,size_t bytes,unsigned char prefix,void *replacement){
	DWORD reset;
	
	if(VirtualProtect(ptr,bytes,PAGE_READWRITE,&reset)){
		*(unsigned char*)ptr = prefix;
		*((unsigned char*)ptr+1) = 0xE8;
		*(int32_t*)((char*)ptr+2) = (int32_t)replacement - ((int32_t)ptr+6);
		if(bytes > 6)
			memset((char*)ptr+6,0x90,bytes-6);
		return VirtualProtect(ptr,bytes,reset,&reset);
	}
	return 0;
}