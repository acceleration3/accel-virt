/* One host GPU's EGL display, initialised once and kept.
 *
 * Found by making a context on each EGL device and asking it which GPU it is,
 * which costs a driver start-up per device: 70 ms for NVIDIA's on an idle host,
 * and over three seconds under load - past dxgkrnl's two-second TdrDelay for
 * the guest's first batch, which waits behind its context's executor (feature
 * 044, T035). So the search runs once, at realize, and a context is only ever
 * made on a display already found.
 *
 * Kept rather than terminated after each context for a second reason: EGL
 * hands every caller of one device the same EGLDisplay, and eglTerminate is not
 * counted, so a context closing its display would take every other context on
 * that GPU with it. One owner per process, then, which is LoaderHostApis. */
#ifndef AV_GL_DISPLAY_HPP
#define AV_GL_DISPLAY_HPP

#include <stdint.h>

namespace av {

class GlContextAttributes;

class GlDisplay {
public:
    static constexpr uint32_t uuid_bytes = 16u;

    GlDisplay();
    ~GlDisplay();
    GlDisplay(const GlDisplay &) = delete;
    GlDisplay &operator=(const GlDisplay &) = delete;

    /* Initialises the display of the GPU `uuid` names. False for a host with no
     * EGL, no device that matches, or a display already open. */
    [[nodiscard]] bool open(const uint8_t *uuid);
    void               close();
    bool               open() const;

    /* A context on this display made with `attributes`, sharing `share`'s
     * objects (null: none), current on nobody; null when the display is not
     * open, the attributes are not valid, or the driver refuses one. Binds the
     * OpenGL API on the calling thread first, because EGL keeps the bound API
     * per thread and a worker never bound it. */
    void *create_context(const GlContextAttributes &attributes, void *share) const;
    /* The EGLDisplay itself, which OpenCL's context properties name beside a
     * GL context when a cl_context is to share its objects: CL_EGL_DISPLAY_KHR
     * is how a driver is told which display the GL context belongs to
     * (feature 053, T019). Null until opened. */
    void *handle() const;
    [[nodiscard]] bool make_current(void *context) const;
    /* Releases the calling thread's current context, then destroys `context`. */
    void  destroy_context(void *context) const;

private:
    void *display_;
};

}  // namespace av

#endif  // AV_GL_DISPLAY_HPP
