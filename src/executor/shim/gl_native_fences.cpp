#include "executor/shim/gl_native_fences.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace av {

GlNativeFences::GlNativeFences() : extensions_(), display_(nullptr), create_(nullptr), destroy_(nullptr), duplicate_(nullptr),
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
    extensions_.configure(display);
    if (!extensions_.lists("EGL_KHR_fence_sync") || !extensions_.lists("EGL_ANDROID_native_fence_sync")) return;
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

void GlNativeFences::give_back()
{
    AV_SCOPE("GlNativeFences::give_back");
    extensions_.forget();
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
