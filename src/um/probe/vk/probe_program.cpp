#include "um/probe/vk/probe_program.hpp"

#include "guest/probe/shared_object_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/probe/vulkan_probe.hpp"
#include "um/probe/cl/windows_cl_entry.hpp"

#include <windows.h>

namespace av {

ProbeProgram::ProbeProgram()
    : kmt_(), heap_(), report_(heap_, "avvkprobe"), vulkan_(&vkGetInstanceProcAddr), cl_(), images_(), window_(),
      frames_()
{
    AV_SCOPE("ProbeProgram::ProbeProgram");
}

uint32_t ProbeProgram::run()
{
    AV_SCOPE("ProbeProgram::run");
    const Status initialized = kmt_.initialize();
    if (initialized == Status::ok) {
        const bool  opened = window_.open();
        VulkanProbe probe(kmt_, report_, vulkan_, images_, frames_, window_.module(),
                          opened ? window_.window() : nullptr);
        probe.run();
        SharedObjectProbe shared(kmt_, report_, vulkan_, &cl_);
        shared.run();
    } else {
        report_.check("program", "initialize", initialized, 0u);
    }
    report_.finish();
    write_report();
    return report_.failed();
}

void ProbeProgram::write_report() const
{
    AV_SCOPE("ProbeProgram::write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(),
                                static_cast<DWORD>(report_.length()), &written, nullptr));
}

}
