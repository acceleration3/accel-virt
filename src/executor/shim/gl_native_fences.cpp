#include "executor/shim/gl_native_fences.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string.h>

namespace av {

GlNativeFences::GlNativeFences() : extensions_(nullptr), display_(nullptr), create_(nullptr), destroy_(nullptr), duplicate_(nullptr),
      flush_(nullptr)
{
    AV_SCOPE("GlNativeFences::GlNativeFences");
}

void GlNativeFences::configure(const GlContext &context)
{
    AV_SCOPE("GlNativeFences::configure");
    give_back();
    void *const display = context.display();
    if (display == nullptr) return;
    extensions_ = eglQueryString(static_cast<EGLDisplay>(display), EGL_EXTENSIONS);
    if (!lists("EGL_KHR_fence_sync") || !lists("EGL_ANDROID_native_fence_sync")) return;
    create_ = reinterpret_cast<void *(*)(void *, unsigned, const int *)>(eglGetProcAddress("eglCreateSyncKHR"));
    destroy_ = reinterpret_cast<unsigned (*)(void *, void *)>(eglGetProcAddress("eglDestroySyncKHR"));
    duplicate_ = reinterpret_cast<int (*)(void *, void *)>(eglGetProcAddress("eglDupNativeFenceFDANDROID"));
    flush_ = reinterpret_cast<void (*)()>(context.resolve("glFlush"));
    if (create_ != nullptr && destroy_ != nullptr && duplicate_ != nullptr && flush_ != nullptr) {
        display_ = display;
        return;
    }
    give_back();
}

bool GlNativeFences::lists(const char *wanted) const
{
    AV_SCOPE("GlNativeFences::lists");
    if (extensions_ == nullptr) return false;
    const size_t length = strlen(wanted);
    for (const char *at = strstr(extensions_, wanted); at != nullptr; at = strstr(at + length, wanted))
        if ((at == extensions_ || at[-1] == ' ') && (at[length] == ' ' || at[length] == '\0')) return true;
    return false;
}

void GlNativeFences::give_back()
{
    AV_SCOPE("GlNativeFences::give_back");
    extensions_ = nullptr;
    display_ = nullptr;
    create_ = nullptr;
    destroy_ = nullptr;
    duplicate_ = nullptr;
    flush_ = nullptr;
}

bool GlNativeFences::available() const
{
    AV_SCOPE("GlNativeFences::available");
    return display_ != nullptr;
}

int GlNativeFences::make() const
{
    AV_SCOPE("GlNativeFences::make");
    if (display_ == nullptr) return -1;
    void *const sync = create_(display_, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (sync == nullptr) return -1;
    flush_();
    const int fd = duplicate_(display_, sync);
    static_cast<void>(destroy_(display_, sync));
    return fd >= 0 ? fd : -1;
}

}
