#include "framework.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace samples {

namespace {
constexpr EGLint max_devices = 16;

using QueryDevices = EGLBoolean (*)(EGLint, EGLDeviceEXT *, EGLint *);
using GetString = const GLubyte *(*)(GLenum);
using GetIntegers = void (*)(GLenum, GLint *);
using GetError = GLenum (*)();

EGLDisplay display = EGL_NO_DISPLAY;
EGLContext context = EGL_NO_CONTEXT;

bool open_on(EGLDisplay on)
{
    if (eglInitialize(on, nullptr, nullptr) != EGL_TRUE || eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) return false;
    const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
                                 EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    const EGLContext made = eglCreateContext(on, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    if (made == EGL_NO_CONTEXT) return false;
    if (eglMakeCurrent(on, EGL_NO_SURFACE, EGL_NO_SURFACE, made) != EGL_TRUE) {
        eglDestroyContext(on, made);
        return false;
    }
    display = on;
    context = made;
    return true;
}

void close_current()
{
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    display = EGL_NO_DISPLAY;
    context = EGL_NO_CONTEXT;
}

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

bool create_context(const char *gpu)
{
    if (gpu == nullptr) gpu = getenv("AV_GL_GPU");
    const auto query = reinterpret_cast<QueryDevices>(eglGetProcAddress("eglQueryDevicesEXT"));
    const auto get_string = reinterpret_cast<GetString>(eglGetProcAddress("glGetString"));
    const auto get_integers = reinterpret_cast<GetIntegers>(eglGetProcAddress("glGetIntegerv"));
    EGLDeviceEXT devices[max_devices];
    EGLint found = 0;
    if (query == nullptr || get_string == nullptr || get_integers == nullptr ||
        query(max_devices, devices, &found) != EGL_TRUE)
        return false;
    EGLint best = -1;
    GLint  most = -1;
    for (EGLint i = 0; i < found; i++) {
        const EGLDisplay on = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
        if (on == EGL_NO_DISPLAY || !open_on(on)) continue;
        const char *renderer = reinterpret_cast<const char *>(get_string(GL_RENDERER));
        GLint memory[4] = {0, 0, 0, 0};
        get_integers(GL_TEXTURE_FREE_MEMORY_ATI, memory);
        if (memory[0] == 0) get_integers(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, memory);
        const auto get_error = reinterpret_cast<GetError>(eglGetProcAddress("glGetError"));
        while (get_error != nullptr && get_error() != 0) {
        }
        fprintf(stderr, "EGL device %d: %s, %d MiB\n", i, renderer != nullptr ? renderer : "(none)", memory[0] / 1024);
        const bool named = renderer != nullptr && gpu != nullptr && strstr(renderer, gpu) != nullptr;
        const bool software = renderer == nullptr || strstr(renderer, "llvmpipe") != nullptr;
        if (named) return true;
        if (gpu == nullptr && !software && memory[0] > most) {
            most = memory[0];
            best = i;
        }
        close_current();
    }
    if (best < 0) return false;
    const EGLDisplay on = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[best], nullptr);
    return on != EGL_NO_DISPLAY && open_on(on);
}

void *gl_proc(const char *name) { return reinterpret_cast<void *>(eglGetProcAddress(name)); }

}

int main(int argc, char **argv) { return samples::run(argc, argv); }
