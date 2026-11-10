#include "executor/vram_ledger.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vram_budget_source.hpp"

#include <time.h>

namespace av {

VramLedger::VramLedger() : total_(0u), policy_(), source_(nullptr), stamp_(0u)
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
    __atomic_store_n(&stamp_, 0u, __ATOMIC_RELAXED);
}

void VramLedger::follow(VramBudgetSource *source)
{
    AV_SCOPE("VramLedger::follow");
    __atomic_store_n(&source_, source, __ATOMIC_RELEASE);
}

bool VramLedger::take(uint64_t bytes)
{
    AV_SCOPE("VramLedger::take");
    refresh();
    return ByteLedger::take(bytes);
}

bool VramLedger::take_session(uint64_t bytes)
{
    AV_SCOPE("VramLedger::take_session");
    refresh();
    return ByteLedger::take_session(bytes);
}

void VramLedger::refresh()
{
    AV_SCOPE("VramLedger::refresh");
    VramBudgetSource *const source = __atomic_load_n(&source_, __ATOMIC_ACQUIRE);
    if (source == nullptr || total_ == 0u) return;
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
    const uint64_t ms = static_cast<uint64_t>(now.tv_sec) * 1000u + static_cast<uint64_t>(now.tv_nsec) / 1000000u + 1u;
    uint64_t       stamp = __atomic_load_n(&stamp_, __ATOMIC_RELAXED);
    if (stamp != 0u && ms - stamp < 200u) return;
    if (!__atomic_compare_exchange_n(&stamp_, &stamp, ms, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) return;
    uint64_t budget = 0u;
    uint64_t usage = 0u;
    const uint64_t policy = policy_.cap_for(total_);
    if (!source->device_local(budget, usage)) {
        set_cap(policy);
        return;
    }
    const uint64_t mine = used();
    const uint64_t elsewhere = usage > mine ? usage - mine : 0u;
    const uint64_t room = budget > elsewhere ? budget - elsewhere : 0u;
    const uint64_t followed = room < policy ? room : policy;
    set_cap(followed < mine ? mine : followed);
}

uint64_t VramLedger::advertised() const
{
    AV_SCOPE("VramLedger::advertised");
    return total_ == 0u ? UINT64_MAX : policy_.cap_for(total_);
}

uint64_t VramLedger::total() const
{
    AV_SCOPE("VramLedger::total");
    return total_;
}

bool VramLedger::invariants_hold() const
{
    AV_SCOPE("VramLedger::invariants_hold");
    if (source_ != nullptr) return true;
    return total_ == 0u ? cap() == UINT64_MAX : cap() == policy_.cap_for(total_);
}

}
