// The guest build's platform: Windows, no C runtime, d3dcompiler_47 for HLSL.
#include "framework.h"

#include <d3dcommon.h>

extern "C" {
int _fltused = 0;
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

char shader_directory[260] = "C:\\rvk\\samples\\shaders";

using D3DCompileFn = HRESULT(WINAPI *)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR,
                                        LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
D3DCompileFn compiler = nullptr;

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

void set_shader_directory(const char *directory) { copy_text(shader_directory, sizeof shader_directory, directory); }

// Compiled here, where Microsoft's compiler is, and kept for the host to load.
bool shader(const char *name, const char *source, const char *entry, const char *target, Blob &bytecode)
{
    if (compiler == nullptr) {
        const HMODULE library = LoadLibraryA("d3dcompiler_47.dll");
        if (library != nullptr) compiler = reinterpret_cast<D3DCompileFn>(GetProcAddress(library, "D3DCompile"));
        if (compiler == nullptr) {
            log("FAIL: no d3dcompiler_47\n");
            return false;
        }
    }
    size_t length = 0;
    while (source[length] != '\0') length++;
    ID3DBlob *code = nullptr, *errors = nullptr;
    /* 1 << 15: D3DCOMPILE_OPTIMIZATION_LEVEL3, which d3dcommon.h does not carry. */
    const HRESULT compiled = compiler(source, length, name, nullptr, nullptr, entry, target,
                                      (1u << 15), 0, &code, &errors);
    if (errors != nullptr) {
        log(name);
        log(": ");
        log(static_cast<const char *>(errors->GetBufferPointer()));
        errors->Release();
    }
    if (FAILED(compiled)) return false;
    bytecode.size = code->GetBufferSize();
    bytecode.data = allocate(bytecode.size);
    memcpy(bytecode.data, code->GetBufferPointer(), bytecode.size);
    code->Release();
    char path[512] = {};
    copy_text(path, sizeof path, shader_directory);
    append_text(path, sizeof path, "\\");
    append_text(path, sizeof path, name);
    append_text(path, sizeof path, ".dxbc");
    write_file(path, bytecode.data, bytecode.size);
    return true;
}

bool create_device(ID3D11Device **device, ID3D11DeviceContext **context)
{
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                       device, nullptr, context));
}

}  // namespace samples

extern "C" void rvkd3d11samples_entry()
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
    CreateDirectoryA("C:\\rvk\\samples", nullptr);
    CreateDirectoryA("C:\\rvk\\samples\\shaders", nullptr);
    ExitProcess(static_cast<UINT>(samples::run(argc, argv)));
}
