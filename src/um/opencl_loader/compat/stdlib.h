/* What the Khronos OpenCL loader reaches for from <stdlib.h>, and no more.
 *
 * The guest build has no C runtime (cmake/windows_user_flags_c.cmake passes
 * /Zl /X), so the loader is compiled against these declarations and linked
 * against loader_crt.c, which answers them over the process heap. Only the
 * names the pinned loader actually calls are here: a name it does not call is
 * a name this build does not have to be right about. */
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

#endif  /* AV_OPENCL_LOADER_STDLIB_H */
