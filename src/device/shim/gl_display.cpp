#include "device/shim/gl_display.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/gl_context_attributes.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string.h>

namespace av {

namespace {

/* This host reports four EGL devices for two GPUs, two of which name the same
 * render node and one of which will not initialise. Neither number is one
 * anybody will meet. */
constexpr EGLint max_devices = 16;

/* glGetUnsignedBytevEXT, which GL_EXT_memory_object defines and which is the
 * only way to ask a GL context which GPU it is on in the terms Vulkan uses. */
constexpr unsigned gl_device_uuid = 0x9597u;

using QueryDevices = EGLBoolean (*)(EGLint, EGLDeviceEXT *, EGLint *);
using PlatformDisplay = EGLDisplay (*)(EGLenum, void *, const EGLAttrib *);
using GetUnsignedBytev = void (*)(unsigned, unsigned char *);

}  // namespace

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

    auto *const query = reinterpret_cast<QueryDevices>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto *const platform_display =
        reinterpret_cast<PlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplay"));
    if (query == nullptr || platform_display == nullptr) return false;

    EGLDeviceEXT devices[max_devices];
    EGLint       found = 0;
    if (query(max_devices, devices, &found) != EGL_TRUE) return false;

    for (EGLint i = 0; i < found && display_ == nullptr; i++) {
        EGLDisplay display = platform_display(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
        /* A device that will not initialise is stepped over: this host has one
         * that answers EGL_NOT_INITIALIZED every time. One that does and is
         * another GPU is left initialised - it may be the display another
         * GlDisplay already holds, and terminating it would end that one. */
        if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) continue;
        display_ = display;
        void *const context = create_context(GlContextAttributes(nullptr), nullptr);
        uint8_t mine[uuid_bytes] = {};
        bool    told = false;
        if (make_current(context)) {
            /* Resolved with a context current, where every EGL resolves a GL
             * name. A driver without GL_EXT_memory_object cannot say which GPU
             * it is, so it is not claimed for any rather than guessed at. */
            auto *const unsigned_bytev =
                reinterpret_cast<GetUnsignedBytev>(eglGetProcAddress("glGetUnsignedBytevEXT"));
            if (unsigned_bytev != nullptr) unsigned_bytev(gl_device_uuid, mine);
            told = unsigned_bytev != nullptr;
        }
        destroy_context(context);
        if (!told || memcmp(mine, uuid, uuid_bytes) != 0) display_ = nullptr;
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

bool GlDisplay::make_current(void *context) const
{
    AV_SCOPE("GlDisplay::make_current");
    if (display_ == nullptr || context == nullptr) return false;
    return eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_TRUE;
}

void GlDisplay::destroy_context(void *context) const
{
    AV_SCOPE("GlDisplay::destroy_context");
    if (display_ == nullptr || context == nullptr) return;
    (void)eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    (void)eglDestroyContext(display_, context);
}

}  // namespace av
