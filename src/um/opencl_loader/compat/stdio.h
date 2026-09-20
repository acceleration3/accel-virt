/* The loader's tracing, which is off unless OCL_ICD_ENABLE_TRACE puts it on.
 * The macros expand to fprintf/fwprintf on stderr, so the names must exist to
 * link; loader_crt.c answers them by writing nothing, because a guest test
 * artifact has no console to trace to and the runner reads the probe's report
 * rather than the loader's chatter. */
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

#endif  /* AV_OPENCL_LOADER_STDIO_H */
