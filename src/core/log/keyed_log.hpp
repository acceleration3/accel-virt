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

    [[nodiscard]] bool first(const char *key, int64_t code);
    void               once(const char *what);
    void               once_name(const char *what, const char *name);
    uint32_t           keys() const;

private:
    struct Key {
        const char *key_;
        int64_t     code_;
    };

    ClaimedRows<Key> keys_;
};

}

#endif
