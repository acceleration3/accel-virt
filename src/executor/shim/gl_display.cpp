#include "executor/shim/gl_display.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "proto/context/device_uuid.hpp"

#include <dlfcn.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdlib.h>

extern "C" int setenv(const char *name, const char *value, int overwrite);

namespace av {

namespace {
constexpr EGLint max_devices = 16;

constexpr unsigned gl_device_uuid = 0x9597u;

using QueryDevices = EGLBoolean (*)(EGLint, EGLDeviceEXT *, EGLint *);
using PlatformDisplay = EGLDisplay (*)(EGLenum, void *, const EGLAttrib *);
using GetUnsignedBytev = void (*)(unsigned, unsigned char *);

}

GlDisplay::GlDisplay() : display_(nullptr)
{
    AV_SCOPE("GlDisplay::GlDisplay");
}

GlDisplay::~GlDisplay()
{
    AV_SCOPE("GlDisplay::~GlDisplay");
    close();
}

bool GlDisplay::open() const
{
    AV_SCOPE("GlDisplay::open");
    return display_ != nullptr;
}

bool GlDisplay::open(const uint8_t *uuid)
{
    AV_SCOPE("GlDisplay::open");
    if (display_ != nullptr || uuid == nullptr) return false;
    setenv("mesa_glthread", "false", 0);
    static_cast<void>(dlopen("libEGL.so.1", RTLD_NOW | RTLD_NOLOAD | RTLD_GLOBAL));

    auto *const query = reinterpret_cast<QueryDevices>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto *const platform_display =
        reinterpret_cast<PlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplay"));
    if (query == nullptr || platform_display == nullptr) return false;

    EGLDeviceEXT devices[max_devices];
    EGLint       found = 0;
    if (query(max_devices, devices, &found) != EGL_TRUE) return false;

    for (EGLint i = 0; i < found && display_ == nullptr; i++) {
        EGLDisplay display = platform_display(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
        if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) continue;
        display_ = display;
        void *const context = create_context(GlContextAttributes(nullptr), nullptr);
        uint8_t mine[DeviceUuid::bytes] = {};
        bool    told = false;
        if (make_current(context)) {
            auto *const unsigned_bytev =
                reinterpret_cast<GetUnsignedBytev>(eglGetProcAddress("glGetUnsignedBytevEXT"));
            if (unsigned_bytev != nullptr) unsigned_bytev(gl_device_uuid, mine);
            told = unsigned_bytev != nullptr;
        }
        destroy_context(context);
        if (!told || !DeviceUuid(mine).equals(uuid)) display_ = nullptr;
    }
    return display_ != nullptr;
}

void GlDisplay::close()
{
    AV_SCOPE("GlDisplay::close");
    if (display_ == nullptr) return;
    (void)eglTerminate(display_);
    display_ = nullptr;
}

void *GlDisplay::create_context(const GlContextAttributes &attributes, void *share) const
{
    AV_SCOPE("GlDisplay::create_context");
    if (display_ == nullptr || !attributes.valid() || eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) return nullptr;
    const EGLContext context = eglCreateContext(display_, EGL_NO_CONFIG_KHR,
                                                share != nullptr ? share : EGL_NO_CONTEXT, attributes.list());
    return context == EGL_NO_CONTEXT ? nullptr : context;
}

void *GlDisplay::handle() const
{
    AV_SCOPE("GlDisplay::handle");
    return display_;
}

bool GlDisplay::make_current(void *context) const
{
    AV_SCOPE("GlDisplay::make_current");
    if (display_ == nullptr || context == nullptr) return false;
    return eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_TRUE;
}

void GlDisplay::release_current() const
{
    AV_SCOPE("GlDisplay::release_current");
    if (display_ == nullptr) return;
    (void)eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void GlDisplay::destroy_context(void *context) const
{
    AV_SCOPE("GlDisplay::destroy_context");
    if (display_ == nullptr || context == nullptr) return;
    if (eglGetCurrentContext() == static_cast<EGLContext>(context))
        (void)eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    (void)eglDestroyContext(display_, context);
}

}
