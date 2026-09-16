#include "guest/suite/guest_report.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

namespace {

constexpr uint32_t code_digits = 8u;

}  // namespace

GuestReport::GuestReport() : GuestReport("rvktest")
{
    AV_SCOPE("GuestReport::GuestReport");
}

GuestReport::GuestReport(const char *program)
    : program_(program), text_{}, buffer_(text_, capacity), passed_(0u), failed_(0u), last_scenario_(""),
      last_step_(""), finished_(false)
    {
        AV_SCOPE("GuestReport::GuestReport");
    }

void GuestReport::check(const char *scenario, const char *step, Status status, uint32_t code)
{
    AV_SCOPE("GuestReport::check");
    line(scenario, step, status == Status::ok, status, code);
}

void GuestReport::check_refused(const char *scenario, const char *step, Status status, uint32_t code)
{
    AV_SCOPE("GuestReport::check_refused");
    line(scenario, step, status != Status::ok, status, code);
}

void GuestReport::measure(const char *scenario, const char *step, Status status, uint32_t measurement)
{
    AV_SCOPE("GuestReport::measure");
    const bool passed = status == Status::ok;
    last_scenario_ = scenario;
    last_step_ = step;
    buffer_.append(passed ? "ok " : "FAIL ");
    buffer_.append(scenario);
    buffer_.append(".");
    buffer_.append(step);
    if (!passed) {
        buffer_.append(" status=");
        buffer_.append_decimal(static_cast<uint64_t>(status));
    }
    buffer_.append(" ");
    buffer_.append_decimal(measurement);
    buffer_.append("\n");
    if (passed) passed_++;
    else failed_++;
}

void GuestReport::line(const char *scenario, const char *step, bool passed, Status status, uint32_t code)
{
    AV_SCOPE("GuestReport::line");
    last_scenario_ = scenario;
    last_step_ = step;
    buffer_.append(passed ? "ok " : "FAIL ");
    buffer_.append(scenario);
    buffer_.append(".");
    buffer_.append(step);
    if (!passed) {
        buffer_.append(" status=");
        buffer_.append_decimal(static_cast<uint64_t>(status));
        buffer_.append(" code=0x");
        buffer_.append_hex(code, code_digits);
    }
    buffer_.append("\n");
    if (passed) passed_++;
    else failed_++;
}

void GuestReport::finish()
{
    AV_SCOPE("GuestReport::finish");
    finished_ = true;
    buffer_.append(program_);
    buffer_.append(": ");
    buffer_.append_decimal(passed_);
    buffer_.append(" passed, ");
    buffer_.append_decimal(failed_);
    buffer_.append(" failed\n");
}

const char *GuestReport::text() const
{
    AV_SCOPE("GuestReport::text");
    return buffer_.text();
}
size_t      GuestReport::length() const
{
    AV_SCOPE("GuestReport::length");
    return buffer_.length();
}
uint32_t    GuestReport::passed() const
{
    AV_SCOPE("GuestReport::passed");
    return passed_;
}
uint32_t    GuestReport::failed() const
{
    AV_SCOPE("GuestReport::failed");
    return failed_;
}

/* The terminator always fits inside the storage. */
void GuestReport::report(StateWriter &writer) const
{
    AV_SCOPE("GuestReport::report");
    writer.open("report", 0u);
    writer.text("program", program_);
    writer.number("passed", passed_);
    writer.number("failed", failed_);
    writer.text("last_scenario", last_scenario_);
    writer.text("last_step", last_step_);
    writer.flag("finished", finished_);
    writer.close();
}

bool GuestReport::invariants_hold() const
{
    AV_SCOPE("GuestReport::invariants_hold");
    return buffer_.length() < capacity;
}

}  // namespace av
