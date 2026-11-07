#include "executor/host_ram_ledger.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostRamLedger::HostRamLedger() : budget_(0u)
{
    AV_SCOPE("HostRamLedger::HostRamLedger");
}

void HostRamLedger::budget(uint64_t bytes)
{
    AV_SCOPE("HostRamLedger::budget");
    budget_ = bytes;
    set_cap(bytes == 0u ? UINT64_MAX : bytes);
}

uint64_t HostRamLedger::budget() const
{
    AV_SCOPE("HostRamLedger::budget");
    return budget_;
}

}
