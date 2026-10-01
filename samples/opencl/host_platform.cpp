// The host build's platform: Linux, the host's own OpenCL through its ICD loader.
#include "framework.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace samples {

namespace {
void *loader = nullptr;
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

void *cl_entry(const char *name)
{
    if (loader == nullptr) {
        const char *path = getenv("AV_OPENCL_LIBRARY");
        loader = dlopen(path != nullptr ? path : "libOpenCL.so.1", RTLD_NOW | RTLD_LOCAL);
        if (loader == nullptr) {
            fprintf(stderr, "%s\n", dlerror());
            return nullptr;
        }
    }
    return dlsym(loader, name);
}

}  // namespace samples

int main(int argc, char **argv) { return samples::run(argc, argv); }
