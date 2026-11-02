#ifndef AV_OPENCL_LOADER_STRING_H
#define AV_OPENCL_LOADER_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef __SIZE_TYPE__ size_t;
#endif

void  *memcpy(void *to, const void *from, size_t bytes);
void  *memset(void *block, int value, size_t bytes);
int    memcmp(const void *first, const void *second, size_t bytes);
size_t strlen(const char *text);
int    strcmp(const char *first, const char *second);
char  *strstr(const char *text, const char *wanted);
char  *strchr(const char *text, int wanted);
size_t wcslen(const unsigned short *text);
int    wcscpy_s(unsigned short *to, size_t elements, const unsigned short *from);
int    wcscat_s(unsigned short *to, size_t elements, const unsigned short *from);

#ifdef __cplusplus
}
#endif

#endif
