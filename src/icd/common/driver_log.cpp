#include "icd/common/driver_log.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"

namespace av::icd {

DriverLog::DriverLog(LogSink &sink, const char *prefix)
    : sink_(sink), prefix_(prefix), keys_(), codes_(), ready_(), count_(0u)
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

bool DriverLog::first(const char *key, int64_t code)
{
    AV_SCOPE("DriverLog::first");
    const uint32_t written = __atomic_load_n(&count_, __ATOMIC_ACQUIRE);
    for (uint32_t row = 0u; row < written && row < max_keys; row++)
        if (__atomic_load_n(&ready_[row], __ATOMIC_ACQUIRE) != 0u && keys_[row] == key && codes_[row] == code)
            return false;
    const uint32_t row = __atomic_fetch_add(&count_, 1u, __ATOMIC_ACQ_REL);
    if (row >= max_keys) {
        __atomic_store_n(&count_, max_keys, __ATOMIC_RELEASE);
        return false;
    }
    keys_[row] = key;
    codes_[row] = code;
    __atomic_store_n(&ready_[row], uint8_t{1}, __ATOMIC_RELEASE);
    return true;
}

void DriverLog::once(const char *what)
{
    AV_SCOPE("DriverLog::once");
    if (first(what, 0)) write(what);
}

void DriverLog::once_name(const char *what, const char *name_of)
{
    AV_SCOPE("DriverLog::once_name");
    if (first(name_of, reinterpret_cast<intptr_t>(what))) name(what, name_of);
}

uint32_t DriverLog::keys() const
{
    AV_SCOPE("DriverLog::keys");
    const uint32_t count = __atomic_load_n(&count_, __ATOMIC_ACQUIRE);
    return count < max_keys ? count : max_keys;
}

}  // namespace av::icd
