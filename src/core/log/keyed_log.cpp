#include "core/log/keyed_log.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

KeyedLog::KeyedLog(LogSink &sink, const char *prefix)
    : DriverLog(sink, prefix), keys_(), codes_(), ready_(), count_(0u)
{
    AV_SCOPE("KeyedLog::KeyedLog");
}

bool KeyedLog::first(const char *key, int64_t code)
{
    AV_SCOPE("KeyedLog::first");
    const uint32_t written = keys();
    for (uint32_t row = 0u; row < written; row++)
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

void KeyedLog::once(const char *what)
{
    AV_SCOPE("KeyedLog::once");
    if (first(what, 0)) write(what);
}

void KeyedLog::once_name(const char *what, const char *name_of)
{
    AV_SCOPE("KeyedLog::once_name");
    if (first(name_of, reinterpret_cast<intptr_t>(what))) name(what, name_of);
}

uint32_t KeyedLog::keys() const
{
    AV_SCOPE("KeyedLog::keys");
    const uint32_t count = __atomic_load_n(&count_, __ATOMIC_ACQUIRE);
    return count < max_keys ? count : max_keys;
}

}  // namespace av
