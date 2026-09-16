#include "um/guest_program.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

GuestProgram::GuestProgram(uint32_t stress_seconds, Run run)
    : kmt_(), report_(), suite_(kmt_, report_, stress_seconds), run_(run)
    {
        AV_SCOPE("GuestProgram::GuestProgram");
    }

uint32_t GuestProgram::run()
{
    AV_SCOPE("GuestProgram::run");
    const Status initialized = kmt_.initialize();
    if (initialized == Status::ok) {
        if (run_ == Run::timeout) static_cast<void>(suite_.run_timeout());
        else if (run_ == Run::rvk) static_cast<void>(suite_.run_rvk());
        else static_cast<void>(suite_.run());
    } else {
        report_.check("program", "initialize", initialized, 0u);
        report_.finish();
    }
    write_report();
    return report_.failed();
}

/* A report the console does not take is still the exit code. */
void GuestProgram::write_report() const
{
    AV_SCOPE("GuestProgram::write_report");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(),
                                static_cast<DWORD>(report_.length()), &written, nullptr));
}

}  // namespace av
