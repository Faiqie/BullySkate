/* DERPY'S SCRIPT LOADER: PATCH UTILITY
	
	these are the functions used for patching the game's code
	code can be replaced with a jump/call to another function
	call replacements take an amount of bytes so that NOP can remove some code
	there's also a prefix version of call replaces for possibly pushing a register
	
*/

#ifndef DSL_PATCH_H
#define DSL_PATCH_H

#ifdef __cplusplus
extern "C" {
#endif

int nullifyInstructions(void *ptr,size_t bytes);
int replaceImmediateValue(void *ptr,void *value);
int replaceImmediateByte(void *ptr,char value);
int replaceCodeWithJump(void *ptr,void *target);
int replaceFunctionCall(void *ptr,size_t bytes,void *replacement);
int replaceFunctionCallWithPrefix(void *ptr,size_t bytes,unsigned char prefix,void *replacement);

#ifdef __cplusplus
}
#endif

#endif