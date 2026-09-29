/* GlShareSource over opengl32: the current context is opengl32's to say, and
 * its share name our OpenGL ICD's, asked through wglGetProcAddress as
 * rvkGetShareName - a name only this repository's ICD answers, so a context
 * another vendor's ICD made is named by nobody. opengl32 is looked for, not
 * loaded: a process that never loaded it has no GL context to share. */
#ifndef AV_WINDOWS_GL_SHARE_SOURCE_HPP
#define AV_WINDOWS_GL_SHARE_SOURCE_HPP

#include "icd/opencl/gl_share_source.hpp"

#include <windows.h>

namespace av {

class WindowsGlShareSource final : public clicd::GlShareSource {
public:
    WindowsGlShareSource();

    uint32_t share_name(void *gl_context) override;

private:
    using CurrentContext = HGLRC(WINAPI *)();
    using ProcAddress = PROC(WINAPI *)(LPCSTR);
    using ShareName = uint32_t(WINAPI *)();

    /* opengl32's two entry points, found once it is loaded. */
    bool resolve();

    CurrentContext current_;
    ProcAddress    proc_address_;
};

}  // namespace av

#endif  // AV_WINDOWS_GL_SHARE_SOURCE_HPP
