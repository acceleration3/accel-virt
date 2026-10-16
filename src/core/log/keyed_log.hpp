/* A DriverLog that writes the lines a caller would otherwise write on every
 * call once per key instead.
 *
 * A key is a string literal - an entry point's name, a slot's, a message -
 * so one key is one pointer, with a code beside it (an error, an HRESULT) for
 * a key that may fail more than one way. The keys are ClaimedRows - appended
 * and read without a lock, because a driver logs from inside its own locks -
 * and grow with the keys met: a log line per distinct key and code is what
 * the application asked for, however many there are. Two threads meeting the
 * same new key at the same moment may both write its line, which a log can
 * afford; a key that cannot be remembered for want of memory is not written,
 * so a driver out of memory does not log on every call. A class that only
 * writes lines holds a DriverLog. */
#ifndef AV_KEYED_LOG_HPP
#define AV_KEYED_LOG_HPP

#include "core/log/driver_log.hpp"
#include "core/support/claimed_rows.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class LogSink;

class KeyedLog : public DriverLog {
public:
    KeyedLog(BlockSource &blocks, LogSink &sink, const char *prefix);
    KeyedLog(const KeyedLog &) = delete;
    KeyedLog &operator=(const KeyedLog &) = delete;

    /* True the first time `key` is met with `code`, and from then on false;
     * false too when it cannot be remembered. */
    [[nodiscard]] bool first(const char *key, int64_t code);
    /* write(what), the first time `what` is met. */
    void               once(const char *what);
    /* name(what, name), the first time `name` is met with `what`. */
    void               once_name(const char *what, const char *name);
    uint32_t           keys() const;

private:
    struct Key {
        const char *key_;
        int64_t     code_;
    };

    ClaimedRows<Key> keys_;
};

}  // namespace av

#endif  // AV_KEYED_LOG_HPP
