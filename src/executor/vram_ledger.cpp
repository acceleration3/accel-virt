#include "executor/vram_ledger.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VramLedger::VramLedger() : total_(0u), policy_(), cap_(UINT64_MAX), used_(0u), refusals_(0u)
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
    __atomic_store_n(&cap_, total_ == 0u ? UINT64_MAX : policy_.cap_for(total_), __ATOMIC_RELEASE);
}

bool VramLedger::take(uint64_t bytes)
{
    AV_SCOPE("VramLedger::take");
    const uint64_t cap = __atomic_load_n(&cap_, __ATOMIC_ACQUIRE);
    uint64_t       used = __atomic_load_n(&used_, __ATOMIC_RELAXED);
    for (;;) {
        if (bytes > cap || used > cap - bytes) {
            __atomic_fetch_add(&refusals_, 1u, __ATOMIC_RELAXED);
            return false;
        }
        if (__atomic_compare_exchange_n(&used_, &used, used + bytes, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            return true;
    }
}

void VramLedger::give(uint64_t bytes)
{
    AV_SCOPE("VramLedger::give");
    uint64_t used = __atomic_load_n(&used_, __ATOMIC_RELAXED);
    while (!__atomic_compare_exchange_n(&used_, &used, bytes > used ? 0u : used - bytes, true, __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED)) {
    }
}

uint64_t VramLedger::total() const
{
    AV_SCOPE("VramLedger::total");
    return total_;
}

uint64_t VramLedger::cap() const
{
    AV_SCOPE("VramLedger::cap");
    return __atomic_load_n(&cap_, __ATOMIC_ACQUIRE);
}

uint64_t VramLedger::used() const
{
    AV_SCOPE("VramLedger::used");
    return __atomic_load_n(&used_, __ATOMIC_ACQUIRE);
}

uint64_t VramLedger::free_bytes() const
{
    AV_SCOPE("VramLedger::free_bytes");
    const uint64_t cap = this->cap();
    const uint64_t used = this->used();
    return used < cap ? cap - used : 0u;
}

uint64_t VramLedger::refusals() const
{
    AV_SCOPE("VramLedger::refusals");
    return __atomic_load_n(&refusals_, __ATOMIC_RELAXED);
}

bool VramLedger::invariants_hold() const
{
    AV_SCOPE("VramLedger::invariants_hold");
    return total_ == 0u ? cap() == UINT64_MAX : cap() == policy_.cap_for(total_);
}

}
