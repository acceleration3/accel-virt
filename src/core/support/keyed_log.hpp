/* A DriverLog that writes the lines a caller would otherwise write on every
 * call once per key instead.
 *
 * A key is a string literal - an entry point's name, a slot's, a message -
 * so one key is one pointer, with a code beside it (an error, an HRESULT) for
 * a key that may fail more than one way. The table is append-only and takes
 * no lock, because a driver logs from inside its own locks: two threads
 * meeting the same new key at the same moment may both write its line, which
 * a log can afford. Past max_keys a line is not written at all rather than
 * flooding the log. The table is 4 KiB, so a class that only writes lines
 * holds a DriverLog. */
#ifndef AV_KEYED_LOG_HPP
#define AV_KEYED_LOG_HPP

#include "core/support/driver_log.hpp"

#include <stdint.h>

namespace av {

class LogSink;
struct KeyedLogProbe;

class KeyedLog : public DriverLog {
public:
    static constexpr uint32_t max_keys = 256u;

    KeyedLog(LogSink &sink, const char *prefix);
    KeyedLog(const KeyedLog &) = delete;
    KeyedLog &operator=(const KeyedLog &) = delete;

    /* True the first time `key` is met with `code`, and from then on false. */
    [[nodiscard]] bool first(const char *key, int64_t code);
    /* write(what), the first time `what` is met. */
    void               once(const char *what);
    /* name(what, name), the first time `name` is met with `what`. */
    void               once_name(const char *what, const char *name);
    uint32_t           keys() const;

private:
    friend struct KeyedLogProbe;

    const char *keys_[max_keys];
    int64_t     codes_[max_keys];
    /* Whether a row's key and code are written: a reader passes a row it is
     * still being written into. */
    uint8_t     ready_[max_keys];
    uint32_t    count_;
};

}  // namespace av

#endif  // AV_KEYED_LOG_HPP
