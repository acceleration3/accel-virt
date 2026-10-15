/* Where an OpenCL context made under cl_khr_gl_sharing finds the OpenGL
 * context it names. The application names it by opengl32's HGLRC, which only
 * opengl32 can turn into a context, and the context is this process's OpenGL
 * ICD's - another DLL, with its own connection to the device - so what this
 * answers is that ICD's share name for it: the D3DKMT handle of the context's
 * ring allocation, which the miniport resolves for a context of the same
 * process.
 *
 * Two implementations: the Windows one in the shim, which asks opengl32 which
 * context is current and our OpenGL ICD for its name, and one in the tests
 * that answers what the test scripted. */
#ifndef AV_CLICD_GL_SHARE_SOURCE_HPP
#define AV_CLICD_GL_SHARE_SOURCE_HPP

#include <stdint.h>

namespace av::clicd {

class GlShareSource {
public:
    GlShareSource(const GlShareSource &) = delete;
    GlShareSource &operator=(const GlShareSource &) = delete;

    /* The share name of `gl_context` when it is the calling thread's current
     * context and one this process's OpenGL ICD made; 0 for any other. */
    virtual uint32_t share_name(void *gl_context) = 0;
    /* The calls the calling thread's current OpenGL context holds - our ICD
     * sends them a batch at a time - run by the host before this returns, so
     * an object they made exists there when OpenCL names it. Nothing with no
     * context of ours current. */
    virtual void     run_held_calls() = 0;

protected:
    GlShareSource() = default;
    ~GlShareSource() = default;
};

}  // namespace av::clicd

#endif  // AV_CLICD_GL_SHARE_SOURCE_HPP
