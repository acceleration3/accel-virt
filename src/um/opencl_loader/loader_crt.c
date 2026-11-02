#include <windows.h>

typedef __SIZE_TYPE__ av_size_t;

void *malloc(av_size_t bytes)
{
    return HeapAlloc(GetProcessHeap(), 0u, (SIZE_T)bytes);
}

void *calloc(av_size_t count, av_size_t size)
{
    av_size_t bytes;
    if (count != 0u && size > (~(av_size_t)0) / count) return 0;
    bytes = count * size;
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)bytes);
}

void free(void *block)
{
    if (block != 0) HeapFree(GetProcessHeap(), 0u, block);
}

void *memcpy(void *to, const void *from, av_size_t bytes)
{
    unsigned char       *out = (unsigned char *)to;
    const unsigned char *in = (const unsigned char *)from;
    av_size_t            index;
    for (index = 0u; index < bytes; ++index) out[index] = in[index];
    return to;
}

void *memset(void *block, int value, av_size_t bytes)
{
    unsigned char *out = (unsigned char *)block;
    av_size_t      index;
    for (index = 0u; index < bytes; ++index) out[index] = (unsigned char)value;
    return block;
}

int memcmp(const void *first, const void *second, av_size_t bytes)
{
    const unsigned char *left = (const unsigned char *)first;
    const unsigned char *right = (const unsigned char *)second;
    av_size_t            index;
    for (index = 0u; index < bytes; ++index) {
        if (left[index] != right[index]) return left[index] < right[index] ? -1 : 1;
    }
    return 0;
}

av_size_t strlen(const char *text)
{
    av_size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

int strcmp(const char *first, const char *second)
{
    av_size_t index = 0u;
    while (first[index] != '\0' && first[index] == second[index]) ++index;
    if (first[index] == second[index]) return 0;
    return (unsigned char)first[index] < (unsigned char)second[index] ? -1 : 1;
}

char *strchr(const char *text, int wanted)
{
    const char want = (char)wanted;
    for (;; ++text) {
        if (*text == want) return (char *)text;
        if (*text == '\0') return 0;
    }
}

av_size_t wcslen(const unsigned short *text)
{
    av_size_t length = 0u;
    while (text[length] != 0) ++length;
    return length;
}

int wcscpy_s(unsigned short *to, av_size_t elements, const unsigned short *from)
{
    av_size_t index;
    if (to == 0 || elements == 0u) return 22;
    for (index = 0u; from[index] != 0; ++index) {
        if (index + 1u >= elements) { to[0] = 0; return 34; }
        to[index] = from[index];
    }
    to[index] = 0;
    return 0;
}

int wcscat_s(unsigned short *to, av_size_t elements, const unsigned short *from)
{
    av_size_t used = 0u;
    if (to == 0 || elements == 0u) return 22;
    while (used < elements && to[used] != 0) ++used;
    if (used == elements) { to[0] = 0; return 22; }
    return wcscpy_s(to + used, elements - used, from);
}

void qsort(void *base, av_size_t count, av_size_t size,
           int (*compare)(const void *, const void *))
{
    unsigned char *items = (unsigned char *)base;
    av_size_t      taken, place, byte;
    for (taken = 1u; taken < count; ++taken) {
        for (place = taken; place > 0u; --place) {
            unsigned char *left = items + (place - 1u) * size;
            unsigned char *right = items + place * size;
            if (compare(left, right) <= 0) break;
            for (byte = 0u; byte < size; ++byte) {
                const unsigned char swap = left[byte];
                left[byte] = right[byte];
                right[byte] = swap;
            }
        }
    }
}

char *getenv(const char *name)
{
    (void)name;
    return 0;
}

struct av_loader_file { int unused; };
static struct av_loader_file av_loader_stderr_storage = {0};
struct av_loader_file *const av_loader_stderr = &av_loader_stderr_storage;

int fprintf(struct av_loader_file *stream, const char *format, ...)
{
    (void)stream;
    (void)format;
    return 0;
}

int fwprintf(struct av_loader_file *stream, const unsigned short *format, ...)
{
    (void)stream;
    (void)format;
    return 0;
}

int __stdcall av_opencl_loader_entry(void *module, unsigned long reason, void *reserved)
{
    (void)module;
    (void)reason;
    (void)reserved;
    return 1;
}
