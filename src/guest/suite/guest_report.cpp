#include "guest/suite/guest_report.hpp"

namespace av {

namespace {

constexpr uint32_t code_digits = 8u;

}  // namespace

GuestReport::GuestReport() : GuestReport("rvktest") {}

GuestReport::GuestReport(const char *program)
    : program_(program), text_{}, buffer_(text_, capacity), passed_(0u), failed_(0u) {}

void GuestReport::check(const char *scenario, const char *step, Status status, uint32_t code)
{
    line(scenario, step, status == Status::ok, status, code);
}

void GuestReport::check_refused(const char *scenario, const char *step, Status status, uint32_t code)
{
    line(scenario, step, status != Status::ok, status, code);
}

void GuestReport::measure(const char *scenario, const char *step, Status status, uint32_t measurement)
{
    const bool passed = status == Status::ok;
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
    buffer_.append(program_);
    buffer_.append(": ");
    buffer_.append_decimal(passed_);
    buffer_.append(" passed, ");
    buffer_.append_decimal(failed_);
    buffer_.append(" failed\n");
}

const char *GuestReport::text() const  { return buffer_.text(); }
size_t      GuestReport::length() const { return buffer_.length(); }
uint32_t    GuestReport::passed() const { return passed_; }
uint32_t    GuestReport::failed() const { return failed_; }

/* The terminator always fits inside the storage. */
bool GuestReport::invariants_hold() const
{
    return buffer_.length() < capacity;
}

}  // namespace av
