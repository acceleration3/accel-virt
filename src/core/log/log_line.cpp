#include "core/log/log_line.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

LogLine::LogLine(DriverLog &log) : log_(log), bytes_{}, body_(bytes_, sizeof bytes_)
{
    AV_SCOPE("LogLine::LogLine");
}

LogLine &LogLine::text(const char *text)
{
    AV_SCOPE("LogLine::text");
    body_.append(text);
    return *this;
}

LogLine &LogLine::number(uint64_t value)
{
    AV_SCOPE("LogLine::number");
    body_.append_decimal(value);
    return *this;
}

LogLine &LogLine::tenths(uint64_t value)
{
    AV_SCOPE("LogLine::tenths");
    constexpr uint64_t tenth = 10u;
    body_.append_decimal(value / tenth);
    body_.append(".");
    body_.append_decimal(value % tenth);
    return *this;
}

LogLine &LogLine::hex(uint64_t value, uint32_t digits)
{
    AV_SCOPE("LogLine::hex");
    body_.append("0x");
    body_.append_hex(value, digits);
    return *this;
}

void LogLine::write()
{
    AV_SCOPE("LogLine::write");
    log_.write(body_.text());
}

}  // namespace av
