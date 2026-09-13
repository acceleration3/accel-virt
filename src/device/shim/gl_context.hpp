/* An OpenGL context on a named host GPU, with no window.
 *
 * The shim's other half. EGL is reached through EGL_EXT_platform_device and
 * eglQueryDevicesEXT rather than EGL_MESA_platform_surfaceless, because
 * surfaceless gives no choice of GPU at all - it lands on whichever the vendor
 * ordering picks, which in the probe was not the one asked for. A context here
 * is asked for by the same UUID the device binds by, and a device that will not
 * initialise is stepped over rather than indexed into (feature 042). */
#ifndef AV_GL_CONTEXT_HPP
#define AV_GL_CONTEXT_HPP

#include <stdint.h>

namespace av {

class GlContext {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    GlContext();
    ~GlContext();
    GlContext(const GlContext &) = delete;
    GlContext &operator=(const GlContext &) = delete;

    /* Opens a context on the GPU `uuid` names. False for a host with no EGL,
     * no device that matches, or no device that will make a context - all of
     * which mean this host does not serve OpenGL for that GPU, which is an
     * answer rather than a failure. */
    [[nodiscard]] bool open(const uint8_t *uuid);
    void               close();
    bool               open() const;

    /* Makes this context current on the calling thread. One context belongs to
     * one worker, so this is called around a dispatch rather than once. */
    [[nodiscard]] bool make_current();
    /* Resolves an entry point, which is what fills the generated table. */
    void              *resolve(const char *name) const;

private:
    void *display_;
    void *context_;
};

}  // namespace av

#endif  // AV_GL_CONTEXT_HPP
