#include "um/avtest/guest_program.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

#include <windows.h>

namespace av {

GuestProgram::GuestProgram(uint32_t stress_seconds, Run run, ObjectStorage<ProcessTrace> &trace,
                           ProcessTrace::Filter crashed)
    : kmt_(), report_(), suite_(kmt_, report_, stress_seconds), run_(run), trace_(trace), crashed_(crashed)
{
    AV_SCOPE("GuestProgram::GuestProgram");
}

uint32_t GuestProgram::run()
{
    AV_SCOPE("GuestProgram::run");
    /* Tracing is a diagnosis aid: the suite runs, and reports only what it
     * tests, whether or not the section could be made. */
    if (trace_.construct() == Status::ok) static_cast<void>(trace_.object().start("avtest", *this, crashed_));
    const Status initialized = kmt_.initialize();
    if (initialized == Status::ok) {
        if (run_ == Run::timeout) static_cast<void>(suite_.run_timeout());
        else if (run_ == Run::av) static_cast<void>(suite_.run_av());
        else if (run_ == Run::round_trip) static_cast<void>(suite_.run_round_trip());
        else if (run_ == Run::host_loss) static_cast<void>(suite_.run_host_loss());
        else static_cast<void>(suite_.run());
    } else {
        report_.check("program", "initialize", initialized, 0u);
        report_.finish();
    }
    write_report();
    return report_.failed();
}

void GuestProgram::report(StateWriter &writer)
{
    AV_SCOPE("GuestProgram::report");
    constexpr const char *runs[] = {"suite", "timeout", "av", "trip", "loss"};
    writer.open("program", 0u);
    writer.text("run", runs[static_cast<uint32_t>(run_)]);
    writer.close();
    report_.report(writer);
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
