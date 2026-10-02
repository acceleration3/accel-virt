/* avvkprobe.exe (feature 045, T039): VulkanProbe over the Khronos loader in
 * vulkan-1.dll - every Vulkan call resolved through its vkGetInstanceProcAddr,
 * the adapter's LUID through gdi32 - its report written to standard output for
 * the guest agent to carry back, its failures the exit code. Translation only:
 * what the probe decides is in src/guest/suite. */
#ifndef AV_PROBE_PROGRAM_HPP
#define AV_PROBE_PROGRAM_HPP

#include "um/probe/cl/windows_cl_entry.hpp"
#include "um/probe/gdi_window_frame.hpp"
#include "um/probe/probe_window.hpp"
#include "um/probe/windows_image_sink.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/probe/loader_vulkan.hpp"
#include "um/kmt/gdi_kmt.hpp"

#include <stdint.h>

namespace av {

class ProbeProgram {
public:
    ProbeProgram();
    ProbeProgram(const ProbeProgram &) = delete;
    ProbeProgram &operator=(const ProbeProgram &) = delete;

    /* The process exit code: how many steps failed. */
    uint32_t run();

private:
    void write_report() const;

    GdiKmt          kmt_;
    GuestReport     report_;
    LoaderVulkan    vulkan_;
    /* The OpenCL beside this process, for the half of the interop matrix only
     * a process with both APIs can walk (feature 053, T019). */
    WindowsClEntry  cl_;
    WindowsImageSink images_;
    /* The window the present steps put their frames on (feature 046). */
    ProbeWindow     window_;
    GdiWindowFrame  frames_;
};

}  // namespace av

#endif  // AV_PROBE_PROGRAM_HPP
