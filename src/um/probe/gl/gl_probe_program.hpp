/* rvkglprobe.exe's proof (feature 050, T042): GlProbe's nine steps over the
 * system opengl32.dll on the probe's own window, `ok opengl.<step>` and
 * `fail opengl.<step>` lines and `rvkglprobe: N passed, M failed` on standard
 * output, the failure count as the exit code. Translation only. */
#ifndef AV_UM_GL_PROBE_PROGRAM_HPP
#define AV_UM_GL_PROBE_PROGRAM_HPP

#include "um/probe/windows_image_sink.hpp"
#include "guest/suite/guest_report.hpp"
#include "um/gdi_kmt.hpp"
#include "um/probe/gl/probe_window.hpp"
#include "um/probe/gl/windows_gl_entry.hpp"

#include <stdint.h>

namespace av {

class GlProbeProgram {
public:
    GlProbeProgram();
    GlProbeProgram(const GlProbeProgram &) = delete;
    GlProbeProgram &operator=(const GlProbeProgram &) = delete;

    uint32_t run();

private:
    void write_report() const;

    GdiKmt         kmt_;
    GuestReport    report_;
    ProbeWindow    window_;
    WindowsGlEntry gl_;
    WindowsImageSink images_;
};

}  // namespace av

#endif  // AV_UM_GL_PROBE_PROGRAM_HPP
