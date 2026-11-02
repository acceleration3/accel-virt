#include "framework.h"

#include <windows.h>

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
    unsigned char *d = static_cast<unsigned char *>(to);
    const unsigned char *s = static_cast<const unsigned char *>(from);
    while (count--) *d++ = *s++;
    return to;
}
void *__cdecl memmove(void *to, const void *from, size_t count)
{
    unsigned char *d = static_cast<unsigned char *>(to);
    const unsigned char *s = static_cast<const unsigned char *>(from);
    if (d < s) while (count--) *d++ = *s++;
    else while (count--) d[count] = s[count];
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

constexpr wchar_t window_class[] = L"avglsamples";
constexpr int wgl_context_major_version = 0x2091, wgl_context_minor_version = 0x2092,
              wgl_context_profile_mask = 0x9126, wgl_context_core_profile_bit = 0x0001;

HMODULE opengl32 = nullptr;

void write_out(const char *text, size_t length)
{
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, static_cast<DWORD>(length), &wrote, nullptr);
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wide, LPARAM lengthy)
{
    return DefWindowProcW(window, message, wide, lengthy);
}

}

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
    DWORD wrote = 0;
    const bool ok = WriteFile(file, data, static_cast<DWORD>(size), &wrote, nullptr) && wrote == size;
    CloseHandle(file);
    return ok;
}

bool read_file(const char *path, Blob &blob)
{
    const HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(file, nullptr);
    blob.data = allocate(size);
    blob.size = size;
    DWORD read = 0;
    const bool ok = ReadFile(file, blob.data, size, &read, nullptr) && read == size;
    CloseHandle(file);
    return ok;
}

bool create_context(const char *)
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW description = {};
    description.cbSize = sizeof description;
    description.style = CS_OWNDC;
    description.lpfnWndProc = &window_procedure;
    description.hInstance = instance;
    description.lpszClassName = window_class;
    if (RegisterClassExW(&description) == 0) return false;
    const HWND window = CreateWindowExW(0u, window_class, L"avglsamples", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr,
                                        instance, nullptr);
    const HDC dc = window != nullptr ? GetDC(window) : nullptr;
    if (dc == nullptr) return false;
    PIXELFORMATDESCRIPTOR wanted = {};
    wanted.nSize = sizeof wanted;
    wanted.nVersion = 1u;
    wanted.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    wanted.iPixelType = PFD_TYPE_RGBA;
    wanted.cColorBits = 32u;
    wanted.cDepthBits = 24u;
    wanted.cStencilBits = 8u;
    const int format = ChoosePixelFormat(dc, &wanted);
    PIXELFORMATDESCRIPTOR described = {};
    if (format == 0 || DescribePixelFormat(dc, format, sizeof described, &described) == 0 ||
        !SetPixelFormat(dc, format, &described))
        return false;
    const HGLRC legacy = wglCreateContext(dc);
    if (legacy == nullptr || !wglMakeCurrent(dc, legacy)) return false;
    using Create = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
    const auto create = reinterpret_cast<Create>(wglGetProcAddress("wglCreateContextAttribsARB"));
    const int attributes[] = {wgl_context_major_version, 4, wgl_context_minor_version, 5, wgl_context_profile_mask,
                              wgl_context_core_profile_bit, 0};
    const HGLRC core = create != nullptr ? create(dc, nullptr, attributes) : nullptr;
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacy);
    if (core == nullptr || !wglMakeCurrent(dc, core)) return false;
    opengl32 = GetModuleHandleW(L"opengl32.dll");
    return true;
}

void *gl_proc(const char *name)
{
    PROC found = wglGetProcAddress(name);
    const auto value = reinterpret_cast<intptr_t>(found);
    if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1)
        found = opengl32 != nullptr ? GetProcAddress(opengl32, name) : nullptr;
    return reinterpret_cast<void *>(found);
}

}

extern "C" void avglsamples_entry()
{
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
    CreateDirectoryA("C:\\av\\glsamples", nullptr);
    ExitProcess(static_cast<UINT>(samples::run(argc, argv)));
}
