// The host build's platform: Linux, vkd3d-proton native, the guest's DXIL.
#include "framework.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace samples {

namespace {
char shader_directory[1024] = "shaders";
void *d3d12 = nullptr;
}

void log(const char *text)
{
    fputs(text, stdout);
    fflush(stdout);
}

void log_number(const char *label, double value) { printf("%s%.3f\n", label, value); }

void *allocate(size_t bytes) { return calloc(1, bytes != 0 ? bytes : 1); }
void  release_memory(void *memory) { free(memory); }

double seconds()
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}

bool write_file(const char *path, const void *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    if (file == nullptr) return false;
    const bool ok = fwrite(data, 1, size, file) == size;
    fclose(file);
    return ok;
}

bool read_file(const char *path, Blob &blob)
{
    FILE *file = fopen(path, "rb");
    if (file == nullptr) return false;
    fseek(file, 0, SEEK_END);
    blob.size = static_cast<size_t>(ftell(file));
    fseek(file, 0, SEEK_SET);
    blob.data = allocate(blob.size);
    const bool ok = fread(blob.data, 1, blob.size, file) == blob.size;
    fclose(file);
    return ok;
}

void set_shader_directory(const char *directory) { copy_text(shader_directory, sizeof shader_directory, directory); }

// The guest compiled it: the same DXIL runs on both machines.
bool shader(const char *name, const char *, const char *, const char *, Blob &bytecode)
{
    char path[1400] = {};
    snprintf(path, sizeof path, "%s/%s.dxil", shader_directory, name);
    if (read_file(path, bytecode)) return true;
    fprintf(stderr, "no DXIL at %s: run the guest build (--compile) first\n", path);
    return false;
}

bool serialize_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc, Blob &blob)
{
    using Serialize = HRESULT(WINAPI *)(const D3D12_ROOT_SIGNATURE_DESC *, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob **,
                                        ID3DBlob **);
    const auto serialize = reinterpret_cast<Serialize>(dlsym(d3d12, "D3D12SerializeRootSignature"));
    ID3DBlob *made = nullptr, *errors = nullptr;
    if (serialize == nullptr || FAILED(serialize(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &made, &errors))) {
        if (errors != nullptr) {
            fprintf(stderr, "root signature: %s\n", static_cast<const char *>(errors->GetBufferPointer()));
            errors->Release();
        }
        return false;
    }
    blob.size = made->GetBufferSize();
    blob.data = allocate(blob.size);
    memcpy(blob.data, made->GetBufferPointer(), blob.size);
    made->Release();
    return true;
}

struct Thread {
    pthread_t handle;
    void (*entry)(void *);
    void *argument;
};

namespace {
void *thread_main(void *thread)
{
    Thread *t = static_cast<Thread *>(thread);
    t->entry(t->argument);
    return nullptr;
}
}  // namespace

Thread *start_thread(void (*entry)(void *), void *argument)
{
    Thread *thread = static_cast<Thread *>(allocate(sizeof(Thread)));
    thread->entry = entry;
    thread->argument = argument;
    if (pthread_create(&thread->handle, nullptr, thread_main, thread) != 0) {
        release_memory(thread);
        return nullptr;
    }
    return thread;
}

void join_thread(Thread *thread)
{
    if (thread == nullptr) return;
    pthread_join(thread->handle, nullptr);
    release_memory(thread);
}

struct Signal {
    pthread_mutex_t lock;
    pthread_cond_t  wake;
    bool            raised;
};

Signal *make_signal()
{
    Signal *signal = static_cast<Signal *>(allocate(sizeof(Signal)));
    pthread_mutex_init(&signal->lock, nullptr);
    pthread_cond_init(&signal->wake, nullptr);
    return signal;
}

void raise_signal(Signal *signal)
{
    pthread_mutex_lock(&signal->lock);
    signal->raised = true;
    pthread_cond_signal(&signal->wake);
    pthread_mutex_unlock(&signal->lock);
}

void wait_signal(Signal *signal)
{
    pthread_mutex_lock(&signal->lock);
    while (!signal->raised) pthread_cond_wait(&signal->wake, &signal->lock);
    signal->raised = false;
    pthread_mutex_unlock(&signal->lock);
}

void release_signal(Signal *signal)
{
    if (signal == nullptr) return;
    pthread_cond_destroy(&signal->wake);
    pthread_mutex_destroy(&signal->lock);
    release_memory(signal);
}

bool create_device(ID3D12Device **device)
{
    setenv("VKD3D_DEBUG", "none", 0);
    setenv("VKD3D_SHADER_DEBUG", "none", 0);
    const char *directory = getenv("AV_D3D_PATH");
    char path[1024] = {};
    // d3d12core first: d3d12 finds it by its soname, and it is not on the library path.
    snprintf(path, sizeof path, "%s/libvkd3d-proton-d3d12core.so", directory != nullptr ? directory : "build-d3d/lib");
    dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    snprintf(path, sizeof path, "%s/libvkd3d-proton-d3d12.so", directory != nullptr ? directory : "build-d3d/lib");
    d3d12 = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (d3d12 == nullptr) {
        fprintf(stderr, "%s: %s\n", path, dlerror());
        return false;
    }
    using Create = HRESULT(WINAPI *)(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **);
    const auto create = reinterpret_cast<Create>(dlsym(d3d12, "D3D12CreateDevice"));
    return create != nullptr &&
           SUCCEEDED(create(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void **>(device)));
}

}  // namespace samples

int main(int argc, char **argv) { return samples::run(argc, argv); }
