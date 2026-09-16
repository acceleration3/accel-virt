#include "um/probe/probe_program.hpp"

#include "guest/suite/vulkan_probe.hpp"

#include <windows.h>

namespace av {

ProbeProgram::ProbeProgram() : kmt_(), report_("rvkvkprobe"), vulkan_(&vkGetInstanceProcAddr) {}

uint32_t ProbeProgram::run()
{
    const Status initialized = kmt_.initialize();
    if (initialized == Status::ok) {
        VulkanProbe probe(kmt_, report_, vulkan_);
        probe.run();
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
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(),
                                static_cast<DWORD>(report_.length()), &written, nullptr));
}

}  // namespace av
