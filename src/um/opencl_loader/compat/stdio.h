#ifndef AV_OPENCL_LOADER_STDIO_H
#define AV_OPENCL_LOADER_STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct av_loader_file FILE;
extern FILE *const av_loader_stderr;
#define stderr av_loader_stderr

int fprintf(FILE *stream, const char *format, ...);
int fwprintf(FILE *stream, const unsigned short *format, ...);

#ifdef __cplusplus
}
#endif

#endif
