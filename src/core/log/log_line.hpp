/* One line for a DriverLog, composed from text and values in the order they
 * are said and written with the log's prefix: a refusal and the several
 * values that decided it, which DriverLog's one-value forms cannot say. Too
 * long a line is cut, never overrun. */
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
    /* A value counted in tenths, written with its one decimal: 125 is 12.5. */
    LogLine &tenths(uint64_t value);
    /* 0x and the low `digits` hexadecimal digits, uppercase. */
    LogLine &hex(uint64_t value, uint32_t digits);
    void     write();

private:
    DriverLog &log_;
    char       bytes_[DriverLog::line_bytes];
    TextBuffer body_;
};

}  // namespace av

#endif  // AV_LOG_LINE_HPP
