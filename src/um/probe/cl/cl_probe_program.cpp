#include "um/probe/cl/cl_probe_program.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/suite/cl_probe.hpp"

#include <windows.h>

namespace av {

ClProbeProgram::ClProbeProgram() : kmt_(), report_("rvkclprobe"), cl_()
{
    AV_SCOPE("ClProbeProgram::ClProbeProgram");
}

uint32_t ClProbeProgram::run()
{
    AV_SCOPE("ClProbeProgram::run");
    const Status initialized = kmt_.initialize();
    if (initialized != Status::ok) {
        report_.check("program", "initialize", initialized, 0u);
    } else {
        ClProbe probe(kmt_, report_, cl_);
        probe.run();
    }
    report_.finish();
    write_report();
    return report_.failed();
}

/* A report the console does not take is still the exit code. */
void ClProbeProgram::write_report() const
{
    AV_SCOPE("ClProbeProgram::write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(),
                                static_cast<DWORD>(report_.length()), &written, nullptr));
}

}  // namespace av
