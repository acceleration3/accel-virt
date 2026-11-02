#ifndef AV_WINDOWS_GL_SHARE_SOURCE_HPP
#define AV_WINDOWS_GL_SHARE_SOURCE_HPP

#include "icd/opencl/gl_share_source.hpp"

#include <windows.h>

namespace av {

class WindowsGlShareSource final : public clicd::GlShareSource {
public:
    WindowsGlShareSource();

    uint32_t share_name(void *gl_context) override;
    void     run_held_calls() override;

private:
    using CurrentContext = HGLRC(WINAPI *)();
    using ProcAddress = PROC(WINAPI *)(LPCSTR);
    using ShareName = uint32_t(WINAPI *)();
    using RunHeldCalls = int32_t(WINAPI *)();

    bool resolve();

    CurrentContext current_;
    ProcAddress    proc_address_;
};

}

#endif
