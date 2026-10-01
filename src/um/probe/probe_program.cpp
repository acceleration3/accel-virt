#include "um/probe/probe_program.hpp"

#include "guest/suite/shared_object_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/vulkan_probe.hpp"
#include "um/probe/cl/windows_cl_entry.hpp"

#include <windows.h>

namespace av {

ProbeProgram::ProbeProgram()
    : kmt_(), report_("avvkprobe"), vulkan_(&vkGetInstanceProcAddr), cl_(), images_(), window_(),
      frames_()
{
    AV_SCOPE("ProbeProgram::ProbeProgram");
}

uint32_t ProbeProgram::run()
{
    AV_SCOPE("ProbeProgram::run");
    const Status initialized = kmt_.initialize();
    if (initialized == Status::ok) {
        /* A window the present steps draw on; without one they do not run, and
         * the rest of the probe still does. */
        const bool  opened = window_.open();
        VulkanProbe probe(kmt_, report_, vulkan_, images_, frames_, window_.module(),
                          opened ? window_.window() : nullptr);
        probe.run();
        /* Feature 053: the three D3DKMT calls nothing else in the guest
         * reaches. Its own instance and device, because the probe above
         * destroys everything it made and these steps need a device asked for
         * with the external-object extensions. */
        /* With the OpenCL loader beside this process, the probe runs the
         * other half of the matrix too: OpenCL opens the handle Vulkan
         * exported and reads what Vulkan wrote (feature 053, T019). Without
         * one, the Vulkan steps run alone. */
        SharedObjectProbe shared(kmt_, report_, vulkan_, &cl_);
        shared.run();
    } else {
        report_.check("program", "initialize", initialized, 0u);
    }
    report_.finish();
    write_report();
    return report_.failed();
}

/* A report the console does not take is still the exit code. */
void ProbeProgram::write_report() const
{
    AV_SCOPE("ProbeProgram::write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(),
                                static_cast<DWORD>(report_.length()), &written, nullptr));
}

}  // namespace av
