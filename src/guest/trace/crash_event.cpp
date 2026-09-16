#include "guest/trace/crash_event.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

CrashEvent::CrashEvent(const char *module, uint32_t pid, uint32_t code, uint64_t address)
    : module_(module != nullptr ? module : ""), pid_(pid), code_(code), address_(address)
{
    AV_SCOPE("CrashEvent::CrashEvent");
}

const char *CrashEvent::module() const
{
    AV_SCOPE("CrashEvent::module");
    return module_;
}

uint32_t CrashEvent::pid() const
{
    AV_SCOPE("CrashEvent::pid");
    return pid_;
}

uint32_t CrashEvent::code() const
{
    AV_SCOPE("CrashEvent::code");
    return code_;
}

uint64_t CrashEvent::address() const
{
    AV_SCOPE("CrashEvent::address");
    return address_;
}

}  // namespace av
