// The guest build's platform: Windows, no C runtime, the Khronos loader beside
// the program (OpenCL.dll), which finds the repository's ICD through the
// display adapter's OpenCLDriverName.
#include <windows.h>

#include "framework.h"

extern "C" {
[[gnu::used]] int _fltused = 0;
int __cdecl _purecall() { ExitProcess(3); }
#pragma function(memset, memcpy, memmove, memcmp)
void *__cdecl memset(void *to, int value, size_t count)
{
    unsigned char *p = static_cast<unsigned char *>(to);
    while (count--) *p++ = static_cast<unsigned char>(value);
    return to;
}
void *__cdecl memcpy(void *to, const void *from, size_t count)
{
    unsigned char       *d = static_cast<unsigned char *>(to);
    const unsigned char *s = static_cast<const unsigned char *>(from);
    while (count--) *d++ = *s++;
    return to;
}
void *__cdecl memmove(void *to, const void *from, size_t count)
{
    unsigned char       *d = static_cast<unsigned char *>(to);
    const unsigned char *s = static_cast<const unsigned char *>(from);
    if (d < s)
        while (count--) *d++ = *s++;
    else
        while (count--) d[count] = s[count];
    return to;
}
int __cdecl memcmp(const void *a, const void *b, size_t count)
{
    const unsigned char *x = static_cast<const unsigned char *>(a), *y = static_cast<const unsigned char *>(b);
    for (; count--; x++, y++)
        if (*x != *y) return *x < *y ? -1 : 1;
    return 0;
}
}

void *operator new(size_t bytes) { return samples::allocate(bytes); }
void  operator delete(void *memory) noexcept { samples::release_memory(memory); }
void  operator delete(void *memory, size_t) noexcept { samples::release_memory(memory); }

namespace samples {

namespace {

HMODULE loader = nullptr;

void write_out(const char *text, size_t length)
{
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, static_cast<DWORD>(length), &wrote, nullptr);
}

}  // namespace

void log(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') length++;
    write_out(text, length);
}

void log_number(const char *label, double value)
{
    char line[128] = {};
    copy_text(line, sizeof line, label);
    append_number(line, sizeof line, value, 3);
    append_text(line, sizeof line, "\n");
    log(line);
}

void *allocate(size_t bytes) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes != 0 ? bytes : 1); }
void  release_memory(void *memory)
{
    if (memory != nullptr) HeapFree(GetProcessHeap(), 0, memory);
}

double seconds()
{
    LARGE_INTEGER frequency = {}, counter = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}

bool write_file(const char *path, const void *data, size_t size)
{
    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD      wrote = 0;
    const bool ok = WriteFile(file, data, static_cast<DWORD>(size), &wrote, nullptr) && wrote == size;
    CloseHandle(file);
    return ok;
}

void *cl_entry(const char *name)
{
    if (loader == nullptr) loader = LoadLibraryA("OpenCL.dll");
    if (loader == nullptr) return nullptr;
    return reinterpret_cast<void *>(GetProcAddress(loader, name));
}

}  // namespace samples

extern "C" void avclsamples_entry()
{
    // The command line split on spaces outside double quotes, as ASCII:
    // shell32's CommandLineToArgvW would be one more import for this.
    const wchar_t *line = GetCommandLineW();
    char         **argv = static_cast<char **>(samples::allocate(sizeof(char *) * 64));
    int            argc = 0;
    while (*line != 0 && argc < 63) {
        while (*line == L' ' || *line == L'\t') line++;
        if (*line == 0) break;
        char  *arg = static_cast<char *>(samples::allocate(1024));
        size_t length = 0;
        bool   quoted = false;
        for (; *line != 0 && (quoted || (*line != L' ' && *line != L'\t')); line++) {
            if (*line == L'"') quoted = !quoted;
            else if (length + 1 < 1024) arg[length++] = static_cast<char>(*line);
        }
        argv[argc++] = arg;
    }
    ExitProcess(static_cast<UINT>(samples::run(argc, argv)));
}
