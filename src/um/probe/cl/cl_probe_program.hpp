/* avclprobe.exe's proof (feature 052, T030): ClProbe's steps over the Khronos
 * loader beside the probe, `ok opencl.<step>` and `FAIL opencl.<step>` lines
 * and `avclprobe: N passed, M failed` on standard output, the failure count
 * as the exit code. No window and no image sink: an OpenCL probe draws
 * nothing. Translation only. */
#ifndef AV_UM_CL_PROBE_PROGRAM_HPP
#define AV_UM_CL_PROBE_PROGRAM_HPP

#include "guest/suite/guest_report.hpp"
#include "um/common/process_heap.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/probe/cl/windows_cl_entry.hpp"

#include <stdint.h>

namespace av {

class ClProbeProgram {
public:
    ClProbeProgram();
    ClProbeProgram(const ClProbeProgram &) = delete;
    ClProbeProgram &operator=(const ClProbeProgram &) = delete;

    uint32_t run();

private:
    void write_report() const;

    GdiKmt         kmt_;
    ProcessHeap    heap_;
    GuestReport    report_;
    WindowsClEntry cl_;
};

}  // namespace av

#endif  // AV_UM_CL_PROBE_PROGRAM_HPP
