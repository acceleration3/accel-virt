#ifndef AV_OVERLAY_PROBE_PROGRAM_HPP
#define AV_OVERLAY_PROBE_PROGRAM_HPP

#include "guest/suite/guest_report.hpp"
#include "um/common/process_heap.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/probe/overlay/windows_overlay_entry.hpp"

#include <stdint.h>

namespace av {

class OverlayProbeProgram {
public:
    explicit OverlayProbeProgram(uint32_t hold);
    OverlayProbeProgram(const OverlayProbeProgram &) = delete;
    OverlayProbeProgram &operator=(const OverlayProbeProgram &) = delete;

    uint32_t run();

private:
    GdiKmt              kmt_;
    ProcessHeap         heap_;
    GuestReport         report_;
    WindowsOverlayEntry dxgi_;
    uint32_t            hold_;
};

}

#endif
