#include "executor/vram_ledger.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/host_ram_ledger.hpp"
#include "executor/vram_budget_source.hpp"

#include <time.h>

namespace av {

VramLedger::VramLedger() : total_(0u), policy_(), source_(nullptr), host_(nullptr), stamp_(0u)
{
    AV_SCOPE("VramLedger::VramLedger");
}

void VramLedger::learn(uint64_t total)
{
    AV_SCOPE("VramLedger::learn");
    total_ = total;
    recompute();
}

void VramLedger::share_host_ram(HostRamLedger *host)
{
    AV_SCOPE("VramLedger::share_host_ram");
    host_ = host;
    recompute();
}

bool VramLedger::shares_host_ram() const
{
    AV_SCOPE("VramLedger::shares_host_ram");
    return host_ != nullptr;
}

uint64_t VramLedger::policy_cap() const
{
    AV_SCOPE("VramLedger::policy_cap");
    if (total_ == 0u) return UINT64_MAX;
    const uint64_t policy = policy_.cap_for(total_);
    const uint64_t host = host_ != nullptr ? host_->budget() : 0u;
    return host != 0u && host < policy ? host : policy;
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
    set_cap(policy_cap());
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
    return ByteLedger::take(bytes) && charge_host(bytes, false);
}

bool VramLedger::take_session(uint64_t bytes)
{
    AV_SCOPE("VramLedger::take_session");
    refresh();
    return ByteLedger::take(bytes) && charge_host(bytes, true);
}

bool VramLedger::charge_host(uint64_t bytes, bool session)
{
    AV_SCOPE("VramLedger::charge_host");
    if (host_ == nullptr || (session ? host_->take_session(bytes) : host_->take(bytes))) return true;
    ByteLedger::give(bytes);
    return false;
}

void VramLedger::give(uint64_t bytes)
{
    AV_SCOPE("VramLedger::give");
    const uint64_t held = used();
    ByteLedger::give(bytes);
    if (host_ != nullptr) host_->give(bytes < held ? bytes : held);
}

void VramLedger::hold(uint64_t bytes)
{
    AV_SCOPE("VramLedger::hold");
    ByteLedger::hold(bytes);
    if (host_ != nullptr) host_->hold(bytes);
}

uint64_t VramLedger::admissible() const
{
    AV_SCOPE("VramLedger::admissible");
    const uint64_t room = free_bytes();
    if (host_ == nullptr) return room;
    const uint64_t host = host_->admissible();
    return host < room ? host : room;
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
    const uint64_t policy = policy_cap();
    if (!source->device_local(budget, usage)) {
        set_cap(policy);
        return;
    }
    const uint64_t mine = used();
    const uint64_t room = budget > usage ? budget - usage + mine : mine;
    const uint64_t followed = room < policy ? room : policy;
    set_cap(followed < mine ? mine : followed);
}

uint64_t VramLedger::advertised() const
{
    AV_SCOPE("VramLedger::advertised");
    return policy_cap();
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
    return cap() == policy_cap();
}

}
