#include "um/probe/gl/gl_probe_program.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/probe/gl_probe.hpp"

#include <windows.h>

namespace av {

GlProbeProgram::GlProbeProgram() : kmt_(), heap_(), report_(heap_, "avglprobe"), window_(), gl_(window_), images_()
{
    AV_SCOPE("GlProbeProgram::GlProbeProgram");
}

uint32_t GlProbeProgram::run()
{
    AV_SCOPE("GlProbeProgram::run");
    const Status initialized = kmt_.initialize();
    if (initialized != Status::ok) {
        report_.check("program", "initialize", initialized, 0u);
    } else if (!window_.open()) {
        report_.check("program", "window", Status::not_found, window_.last_error());
    } else {
        GlProbe probe(kmt_, report_, gl_, images_);
        probe.run();
    }
    report_.finish();
    write_report();
    return report_.failed();
}

/* A report the console does not take is still the exit code. */
void GlProbeProgram::write_report() const
{
    AV_SCOPE("GlProbeProgram::write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(), static_cast<DWORD>(report_.length()),
                                &written, nullptr));
}

}  // namespace av
