// The host build's platform: Linux, DXVK native, the guest's shader bytecode.
#include "framework.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace samples {

namespace {
char shader_directory[1024] = "shaders";
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

// The guest compiled it: the same bytecode runs on both machines.
bool shader(const char *name, const char *, const char *, const char *, Blob &bytecode)
{
    char path[1400] = {};
    snprintf(path, sizeof path, "%s/%s.dxbc", shader_directory, name);
    if (read_file(path, bytecode)) return true;
    fprintf(stderr, "no bytecode at %s: run the guest build (--compile) first\n", path);
    return false;
}

bool create_device(ID3D11Device **device, ID3D11DeviceContext **context)
{
    setenv("DXVK_WSI_DRIVER", "SDL3", 0);
    setenv("DXVK_LOG_PATH", "none", 0);
    const char *directory = getenv("RVK_D3D_PATH");
    char path[1024] = {};
    // dxgi first, global: d3d11 needs it and it is not on the library path.
    snprintf(path, sizeof path, "%s/libdxvk_dxgi.so", directory != nullptr ? directory : "build-d3d/lib");
    dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    snprintf(path, sizeof path, "%s/libdxvk_d3d11.so", directory != nullptr ? directory : "build-d3d/lib");
    void *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        fprintf(stderr, "%s: %s\n", path, dlerror());
        return false;
    }
    using Create = HRESULT(WINAPI *)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT,
                                     UINT, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
    const auto create = reinterpret_cast<Create>(dlsym(library, "D3D11CreateDevice"));
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    return create != nullptr && SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                                 D3D11_SDK_VERSION, device, nullptr, context));
}

}  // namespace samples

int main(int argc, char **argv) { return samples::run(argc, argv); }
