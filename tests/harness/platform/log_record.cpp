#include "harness/platform/log_record.hpp"

#include <cstdio>

namespace av {

LogRecord::LogRecord() : count_(0u), last_{} {}

void LogRecord::write(const char *message)
{
    count_++;
    std::snprintf(last_, sizeof last_, "%s", message);
}

std::uint32_t LogRecord::count() const
{
    return count_;
}

const char *LogRecord::last() const
{
    return last_;
}

}  // namespace av
