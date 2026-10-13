/* avoverlayprobe.exe: the overlay proof (src/guest/probe/overlay_work.hpp) on
 * the guest's own Direct3D 11 and DWM, its report written to standard output
 * and its failures counted in the exit code. */
#ifndef AV_OVERLAY_PROBE_PROGRAM_HPP
#define AV_OVERLAY_PROBE_PROGRAM_HPP

#include "guest/suite/guest_report.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/probe/overlay/windows_overlay_entry.hpp"

#include <stdint.h>

namespace av {

class OverlayProbeProgram {
public:
    OverlayProbeProgram();
    OverlayProbeProgram(const OverlayProbeProgram &) = delete;
    OverlayProbeProgram &operator=(const OverlayProbeProgram &) = delete;

    /* The failures, 0 for a clean run. */
    uint32_t run();

private:
    GdiKmt              kmt_;
    GuestReport         report_;
    WindowsOverlayEntry dxgi_;
};

}  // namespace av

#endif  // AV_OVERLAY_PROBE_PROGRAM_HPP
