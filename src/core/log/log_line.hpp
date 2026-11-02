#ifndef AV_LOG_LINE_HPP
#define AV_LOG_LINE_HPP

#include "core/log/driver_log.hpp"
#include "core/support/text_buffer.hpp"

#include <stdint.h>

namespace av {

class LogLine {
public:
    explicit LogLine(DriverLog &log);
    LogLine(const LogLine &) = delete;
    LogLine &operator=(const LogLine &) = delete;

    LogLine &text(const char *text);
    LogLine &number(uint64_t value);
    LogLine &tenths(uint64_t value);
    LogLine &hex(uint64_t value, uint32_t digits);
    void     write();

private:
    DriverLog &log_;
    char       bytes_[DriverLog::line_bytes];
    TextBuffer body_;
};

}

#endif
