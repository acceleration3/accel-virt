#ifndef AV_OPENCL_LOADER_STDLIB_H
#define AV_OPENCL_LOADER_STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

typedef __SIZE_TYPE__ size_t_av;
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef __SIZE_TYPE__ size_t;
#endif

void *malloc(size_t bytes);
void *calloc(size_t count, size_t size);
void  free(void *block);
void  qsort(void *base, size_t count, size_t size, int (*compare)(const void *, const void *));
char *getenv(const char *name);

#ifdef __cplusplus
}
#endif

#endif
