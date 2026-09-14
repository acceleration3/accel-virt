/* An OpenGL context on a host GPU's display, with no window.
 *
 * The shim's other half. The display is found once and kept by GlDisplay; what
 * a guest context costs here is one eglCreateContext on it, a millisecond or
 * so, rather than a driver start-up per EGL device (feature 044, T035). */
#ifndef AV_GL_CONTEXT_HPP
#define AV_GL_CONTEXT_HPP

namespace av {

class GlDisplay;

class GlContext {
public:
    GlContext();
    ~GlContext();
    GlContext(const GlContext &) = delete;
    GlContext &operator=(const GlContext &) = delete;

    /* Opens a context on `display`, which must outlive it. False for a display
     * that is not open, a driver that refuses a context, or a context already
     * open. */
    [[nodiscard]] bool open(const GlDisplay &display);
    /* Destroys the context; the display stays open for every other one. */
    void               close();
    bool               open() const;

    /* Makes this context current on the calling thread. One context belongs to
     * one worker, so this is called around a dispatch rather than once. */
    [[nodiscard]] bool make_current();
    /* Resolves an entry point, which is what fills the generated table. */
    void              *resolve(const char *name) const;

private:
    const GlDisplay *display_;
    void            *context_;
};

}  // namespace av

#endif  // AV_GL_CONTEXT_HPP
