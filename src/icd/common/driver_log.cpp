#include "icd/common/driver_log.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"

namespace av::icd {

DriverLog::DriverLog(LogSink &sink, const char *prefix)
    : sink_(sink), prefix_(prefix)
{
    AV_SCOPE("DriverLog::DriverLog");
}

void DriverLog::write(const char *what)
{
    AV_SCOPE("DriverLog::write");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append(prefix_);
    text.append(what);
    sink_.write(text.text());
}

void DriverLog::note(const char *what, uint64_t value)
{
    AV_SCOPE("DriverLog::note");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append(prefix_);
    text.append(what);
    text.append(" ");
    text.append_decimal(value);
    sink_.write(text.text());
}

void DriverLog::note_hex(const char *what, uint64_t value, uint32_t digits)
{
    AV_SCOPE("DriverLog::note_hex");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append(prefix_);
    text.append(what);
    text.append(" 0x");
    text.append_hex(value, digits);
    sink_.write(text.text());
}

void DriverLog::name(const char *what, const char *name)
{
    AV_SCOPE("DriverLog::name");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append(prefix_);
    text.append(what);
    text.append(" ");
    text.append(name);
    sink_.write(text.text());
}

}  // namespace av::icd
