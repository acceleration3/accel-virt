#include "core/log/keyed_log.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

KeyedLog::KeyedLog(BlockSource &blocks, LogSink &sink, const char *prefix)
    : DriverLog(sink, prefix), keys_(blocks, Pool::paged)
{
    AV_SCOPE("KeyedLog::KeyedLog");
}

bool KeyedLog::first(const char *key, int64_t code)
{
    AV_SCOPE("KeyedLog::first");
    if (keys_.find_if([key, code](const Key &met) { return met.key_ == key && met.code_ == code; }) != nullptr)
        return false;
    return keys_.append(Key{key, code}) == Status::ok;
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
    return keys_.ready();
}

}
