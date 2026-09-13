#include "device/shim/gl_context.hpp"

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

GlContext::GlContext() : display_(nullptr), context_(nullptr) {}

GlContext::~GlContext() { close(); }

bool GlContext::open() const { return context_ != nullptr; }

bool GlContext::open(const uint8_t *uuid)
{
    if (context_ != nullptr || uuid == nullptr) return false;

    auto *const query =
        reinterpret_cast<QueryDevices>(eglGetProcAddress("eglQueryDevicesEXT"));
    auto *const platform_display =
        reinterpret_cast<PlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplay"));
    if (query == nullptr || platform_display == nullptr) return false;

    EGLDeviceEXT devices[max_devices];
    EGLint       found = 0;
    if (query(max_devices, devices, &found) != EGL_TRUE) return false;

    for (EGLint i = 0; i < found; i++) {
        EGLDisplay display = platform_display(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
        if (display == EGL_NO_DISPLAY) continue;
        /* A device that will not initialise is stepped over: this host has one
         * that answers EGL_NOT_INITIALIZED every time. */
        if (eglInitialize(display, nullptr, nullptr) != EGL_TRUE) continue;
        if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
            (void)eglTerminate(display);
            continue;
        }
        EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, nullptr);
        if (context == EGL_NO_CONTEXT) {
            (void)eglTerminate(display);
            continue;
        }
        if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) != EGL_TRUE) {
            (void)eglDestroyContext(display, context);
            (void)eglTerminate(display);
            continue;
        }

        /* Which GPU this turned out to be, in the terms the device named it in.
         * A driver without GL_EXT_memory_object cannot say, so it is not
         * claimed for any GPU rather than guessed at. */
        auto *const unsigned_bytev =
            reinterpret_cast<GetUnsignedBytev>(eglGetProcAddress("glGetUnsignedBytevEXT"));
        uint8_t mine[uuid_bytes] = {};
        if (unsigned_bytev != nullptr) unsigned_bytev(gl_device_uuid, mine);
        if (unsigned_bytev != nullptr && memcmp(mine, uuid, uuid_bytes) == 0) {
            display_ = display;
            context_ = context;
            return true;
        }
        (void)eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        (void)eglDestroyContext(display, context);
        (void)eglTerminate(display);
    }
    return false;
}

bool GlContext::make_current()
{
    if (context_ == nullptr) return false;
    return eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_) == EGL_TRUE;
}

void *GlContext::resolve(const char *name) const
{
    if (context_ == nullptr) return nullptr;
    return reinterpret_cast<void *>(eglGetProcAddress(name));
}

void GlContext::close()
{
    if (context_ == nullptr) return;
    (void)eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    (void)eglDestroyContext(display_, context_);
    (void)eglTerminate(display_);
    display_ = nullptr;
    context_ = nullptr;
}

}  // namespace av
