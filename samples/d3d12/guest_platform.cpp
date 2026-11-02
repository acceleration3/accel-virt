#include "framework.h"

#include <d3dcommon.h>

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

struct DxcBuffer {
    LPCVOID Ptr;
    SIZE_T  Size;
    UINT    Encoding;
};
MIDL_INTERFACE("8BA5FB08-5195-40e2-AC58-0D989C3A0102")
IDxcBlob : public IUnknown {
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};
MIDL_INTERFACE("7241d424-2646-4191-97c0-98e96e42fc68")
IDxcBlobEncoding : public IDxcBlob {
    virtual HRESULT STDMETHODCALLTYPE GetEncoding(BOOL *known, UINT32 *code_page) = 0;
};
MIDL_INTERFACE("CEDB484A-D4E9-445A-B991-CA21CA157DC2")
IDxcOperationResult : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetStatus(HRESULT *status) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResult(IDxcBlob **result) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetErrorBuffer(IDxcBlobEncoding **errors) = 0;
};
MIDL_INTERFACE("228B4687-5A6A-4730-900C-9702B2203F54")
IDxcCompiler3 : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Compile(const DxcBuffer *source, LPCWSTR *arguments, UINT32 count,
                                              IUnknown *include_handler, REFIID riid, LPVOID *result) = 0;
    virtual HRESULT STDMETHODCALLTYPE Disassemble(const DxcBuffer *object, REFIID riid, LPVOID *result) = 0;
};

namespace samples {

namespace {

char shader_directory[260] = "C:\\av\\samples12\\shaders";

const GUID clsid_dxc_compiler = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};
const GUID iid_dxc_compiler3 = {0x228b4687, 0x5a6a, 0x4730, {0x90, 0x0c, 0x97, 0x02, 0xb2, 0x20, 0x3f, 0x54}};
const GUID iid_dxc_operation_result = {0xcedb484a, 0xd4e9, 0x445a, {0xb9, 0x91, 0xca, 0x21, 0xca, 0x15, 0x7d, 0xc2}};

using DxcCreateInstanceFn = HRESULT(WINAPI *)(REFCLSID, REFIID, LPVOID *);
IDxcCompiler3 *compiler = nullptr;

void write_out(const char *text, size_t length)
{
    DWORD wrote = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, static_cast<DWORD>(length), &wrote, nullptr);
}

void widen(wchar_t *to, size_t capacity, const char *from)
{
    size_t i = 0;
    for (; i + 1 < capacity && from[i] != '\0'; i++) to[i] = static_cast<wchar_t>(from[i]);
    to[i] = 0;
}

bool load_compiler()
{
    if (compiler != nullptr) return true;
    const HMODULE library = LoadLibraryA("dxcompiler.dll");
    const auto create = library != nullptr
                            ? reinterpret_cast<DxcCreateInstanceFn>(GetProcAddress(library, "DxcCreateInstance"))
                            : nullptr;
    if (create == nullptr || FAILED(create(clsid_dxc_compiler, iid_dxc_compiler3, reinterpret_cast<void **>(&compiler)))) {
        log("FAIL: no dxcompiler.dll beside the program\n");
        compiler = nullptr;
        return false;
    }
    return true;
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

void set_shader_directory(const char *directory) { copy_text(shader_directory, sizeof shader_directory, directory); }

bool shader(const char *name, const char *source, const char *entry, const char *target, Blob &bytecode)
{
    if (!load_compiler()) return false;
    wchar_t wide_entry[64], wide_target[16], wide_name[64];
    widen(wide_entry, 64, entry);
    widen(wide_target, 16, target);
    widen(wide_name, 64, name);
    LPCWSTR arguments[] = {wide_name, L"-E", wide_entry, L"-T", wide_target, L"-O3", L"-HV", L"2018"};
    size_t length = 0;
    while (source[length] != '\0') length++;
    const DxcBuffer text = {source, length, 65001u };
    IDxcOperationResult *compiled = nullptr;
    if (FAILED(compiler->Compile(&text, arguments, sizeof arguments / sizeof arguments[0], nullptr,
                                 iid_dxc_operation_result, reinterpret_cast<void **>(&compiled))))
        return false;
    HRESULT status = E_FAIL;
    compiled->GetStatus(&status);
    IDxcBlobEncoding *errors = nullptr;
    if (SUCCEEDED(compiled->GetErrorBuffer(&errors)) && errors != nullptr) {
        if (errors->GetBufferSize() > 1) {
            log(name);
            log(": ");
            char *message = static_cast<char *>(allocate(errors->GetBufferSize() + 1));
            copy_bytes(message, errors->GetBufferPointer(), errors->GetBufferSize());
            log(message);
            release_memory(message);
        }
        errors->Release();
    }
    IDxcBlob *code = nullptr;
    if (FAILED(status) || FAILED(compiled->GetResult(&code)) || code == nullptr) {
        compiled->Release();
        return false;
    }
    bytecode.size = code->GetBufferSize();
    bytecode.data = allocate(bytecode.size);
    copy_bytes(bytecode.data, code->GetBufferPointer(), bytecode.size);
    code->Release();
    compiled->Release();
    char path[512] = {};
    copy_text(path, sizeof path, shader_directory);
    append_text(path, sizeof path, "\\");
    append_text(path, sizeof path, name);
    append_text(path, sizeof path, ".dxil");
    write_file(path, bytecode.data, bytecode.size);
    return true;
}

bool serialize_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc, Blob &blob)
{
    ID3DBlob *made = nullptr, *errors = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &made, &errors))) {
        if (errors != nullptr) {
            log("root signature: ");
            log(static_cast<const char *>(errors->GetBufferPointer()));
            log("\n");
            errors->Release();
        }
        return false;
    }
    blob.size = made->GetBufferSize();
    blob.data = allocate(blob.size);
    copy_bytes(blob.data, made->GetBufferPointer(), blob.size);
    made->Release();
    return true;
}

struct Thread {
    HANDLE handle;
    void (*entry)(void *);
    void *argument;
};

namespace {
DWORD WINAPI thread_main(LPVOID thread)
{
    Thread *t = static_cast<Thread *>(thread);
    t->entry(t->argument);
    return 0;
}
}

Thread *start_thread(void (*entry)(void *), void *argument)
{
    Thread *thread = static_cast<Thread *>(allocate(sizeof(Thread)));
    thread->entry = entry;
    thread->argument = argument;
    thread->handle = CreateThread(nullptr, 256 * 1024, thread_main, thread, 0, nullptr);
    if (thread->handle == nullptr) {
        release_memory(thread);
        return nullptr;
    }
    return thread;
}

void join_thread(Thread *thread)
{
    if (thread == nullptr) return;
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    release_memory(thread);
}

struct Signal {
    HANDLE event;
};

Signal *make_signal()
{
    Signal *signal = static_cast<Signal *>(allocate(sizeof(Signal)));
    signal->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return signal;
}

void raise_signal(Signal *signal) { SetEvent(signal->event); }
void wait_signal(Signal *signal) { WaitForSingleObject(signal->event, INFINITE); }

void release_signal(Signal *signal)
{
    if (signal == nullptr) return;
    CloseHandle(signal->event);
    release_memory(signal);
}

bool create_device(ID3D12Device **device)
{
    return SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                       reinterpret_cast<void **>(device)));
}

}

extern "C" void avd3d12samples_entry()
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
    CreateDirectoryA("C:\\av\\samples12", nullptr);
    CreateDirectoryA("C:\\av\\samples12\\shaders", nullptr);
    ExitProcess(static_cast<UINT>(samples::run(argc, argv)));
}
