#ifndef AV_UM_GL_PROBE_PROGRAM_HPP
#define AV_UM_GL_PROBE_PROGRAM_HPP

#include "um/probe/windows_image_sink.hpp"
#include "guest/suite/guest_report.hpp"
#include "um/common/process_heap.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/probe/probe_window.hpp"
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
    ProcessHeap    heap_;
    GuestReport    report_;
    ProbeWindow    window_;
    WindowsGlEntry gl_;
    WindowsImageSink images_;
};

}

#endif
