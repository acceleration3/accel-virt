#include "executor/vram_ledger.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VramLedger::VramLedger() : total_(0u), policy_()
{
    AV_SCOPE("VramLedger::VramLedger");
}

void VramLedger::learn(uint64_t total)
{
    AV_SCOPE("VramLedger::learn");
    total_ = total;
    recompute();
}

void VramLedger::reserve_vram(uint32_t mib)
{
    AV_SCOPE("VramLedger::reserve_vram");
    policy_ = VramPolicy(mib);
    recompute();
}

void VramLedger::recompute()
{
    AV_SCOPE("VramLedger::recompute");
    set_cap(total_ == 0u ? UINT64_MAX : policy_.cap_for(total_));
}

uint64_t VramLedger::total() const
{
    AV_SCOPE("VramLedger::total");
    return total_;
}

bool VramLedger::invariants_hold() const
{
    AV_SCOPE("VramLedger::invariants_hold");
    return total_ == 0u ? cap() == UINT64_MAX : cap() == policy_.cap_for(total_);
}

}
