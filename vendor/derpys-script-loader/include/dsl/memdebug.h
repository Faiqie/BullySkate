#define malloc(bytes) dslmalloc(bytes)

#ifdef __cplusplus
extern "C"
#endif
void* dslmalloc(size_t);