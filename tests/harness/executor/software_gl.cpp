#include "harness/executor/software_gl.hpp"

#include "core/trace/function_scope.hpp"

#include <cstring>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace av {

namespace {

constexpr unsigned gl_renderer = 0x1F01u;
constexpr unsigned gl_device_uuid = 0x9597u;

using GetString = const unsigned char *(*)(unsigned);
using GetUnsignedBytev = void (*)(unsigned, unsigned char *);

}

SoftwareGl::SoftwareGl() : display_(), uuid_{}, usable_(false)
{
    AV_SCOPE("SoftwareGl::SoftwareGl");
    probe();
}

bool SoftwareGl::usable() const
{
    AV_SCOPE("SoftwareGl::usable");
    return usable_;
}

const GlDisplay *SoftwareGl::display() const
{
    AV_SCOPE("SoftwareGl::display");
    return &display_;
}

const std::uint8_t *SoftwareGl::uuid() const
{
    AV_SCOPE("SoftwareGl::uuid");
    return uuid_;
}

void SoftwareGl::probe()
{
    AV_SCOPE("SoftwareGl::probe");
    if (eglGetProcAddress("eglQueryDevicesEXT") == nullptr) return;
    auto *const query = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto *const platform =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    auto *const device_string =
        reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
    EGLDeviceEXT devices[8];
    EGLint       found = 0;
    if (query == nullptr || platform == nullptr || device_string == nullptr ||
        query(8, devices, &found) != EGL_TRUE)
        return;
    for (EGLint index = 0; index < found && !usable_; index++) {
        const char *const extensions = device_string(devices[index], EGL_EXTENSIONS);
        if (extensions == nullptr || std::strstr(extensions, "EGL_MESA_device_software") == nullptr) continue;
        EGLDisplay display = platform(EGL_PLATFORM_DEVICE_EXT, devices[index], nullptr);
        if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) continue;
        eglBindAPI(EGL_OPENGL_API);
        EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, nullptr);
        if (context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
            auto *const get_string = reinterpret_cast<GetString>(eglGetProcAddress("glGetString"));
            auto *const unsigned_bytev =
                reinterpret_cast<GetUnsignedBytev>(eglGetProcAddress("glGetUnsignedBytevEXT"));
            const char *const renderer = get_string != nullptr
                                             ? reinterpret_cast<const char *>(get_string(gl_renderer))
                                             : nullptr;
            if (renderer != nullptr && std::strstr(renderer, "llvmpipe") != nullptr && unsigned_bytev != nullptr) {
                unsigned_bytev(gl_device_uuid, uuid_);
                usable_ = true;
            }
        }
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
    }
    usable_ = usable_ && display_.open(uuid_);
}

}
