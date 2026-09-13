#include "um/guest_program.hpp"

#include <windows.h>

namespace av {

GuestProgram::GuestProgram(uint32_t stress_seconds, bool timeout_only)
    : kmt_(), report_(), suite_(kmt_, report_, stress_seconds), timeout_only_(timeout_only) {}

uint32_t GuestProgram::run()
{
    const Status initialized = kmt_.initialize();
    if (initialized == Status::ok) {
        static_cast<void>(timeout_only_ ? suite_.run_timeout() : suite_.run());
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
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(),
                                static_cast<DWORD>(report_.length()), &written, nullptr));
}

}  // namespace av
