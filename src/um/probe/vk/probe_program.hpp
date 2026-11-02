#ifndef AV_PROBE_PROGRAM_HPP
#define AV_PROBE_PROGRAM_HPP

#include "um/probe/cl/windows_cl_entry.hpp"
#include "um/probe/gdi_window_frame.hpp"
#include "um/probe/probe_window.hpp"
#include "um/probe/windows_image_sink.hpp"
#include "guest/suite/guest_report.hpp"
#include "um/common/process_heap.hpp"
#include "guest/probe/loader_vulkan.hpp"
#include "um/kmt/gdi_kmt.hpp"

#include <stdint.h>

namespace av {

class ProbeProgram {
public:
    ProbeProgram();
    ProbeProgram(const ProbeProgram &) = delete;
    ProbeProgram &operator=(const ProbeProgram &) = delete;

    uint32_t run();

private:
    void write_report() const;

    GdiKmt          kmt_;
    ProcessHeap     heap_;
    GuestReport     report_;
    LoaderVulkan    vulkan_;
    WindowsClEntry  cl_;
    WindowsImageSink images_;
    ProbeWindow     window_;
    GdiWindowFrame  frames_;
};

}

#endif
