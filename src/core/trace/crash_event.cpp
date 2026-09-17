#include "core/trace/crash_event.hpp"


namespace av {

CrashEvent::CrashEvent(const char *module, uint32_t pid, uint32_t code, uint64_t address)
    : module_(module != nullptr ? module : ""), pid_(pid), code_(code), address_(address)
{}

const char *CrashEvent::module() const
{
    return module_;
}

uint32_t CrashEvent::pid() const
{
    return pid_;
}

uint32_t CrashEvent::code() const
{
    return code_;
}

uint64_t CrashEvent::address() const
{
    return address_;
}

}  // namespace av
