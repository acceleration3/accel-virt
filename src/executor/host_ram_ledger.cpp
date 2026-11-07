#include "executor/host_ram_ledger.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/host_memory_gauge.hpp"

namespace av {

HostRamLedger::HostRamLedger()
    : budget_(0u), gauge_(nullptr), floor_(0u), stamp_(never), available_(never), relieved_(never), tight_refusals_(0u),
      tight_(0u)
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

void HostRamLedger::watch(HostMemoryGauge *gauge)
{
    AV_SCOPE("HostRamLedger::watch");
    __atomic_store_n(&gauge_, gauge, __ATOMIC_RELEASE);
}

void HostRamLedger::floor(uint64_t bytes)
{
    AV_SCOPE("HostRamLedger::floor");
    __atomic_store_n(&stamp_, never, __ATOMIC_RELAXED);
    __atomic_store_n(&floor_, bytes, __ATOMIC_RELEASE);
}

uint64_t HostRamLedger::floor() const
{
    AV_SCOPE("HostRamLedger::floor");
    return __atomic_load_n(&floor_, __ATOMIC_ACQUIRE);
}

uint64_t HostRamLedger::available() const
{
    AV_SCOPE("HostRamLedger::available");
    return __atomic_load_n(&available_, __ATOMIC_RELAXED);
}

uint64_t HostRamLedger::tight_refusals() const
{
    AV_SCOPE("HostRamLedger::tight_refusals");
    return __atomic_load_n(&tight_refusals_, __ATOMIC_RELAXED);
}

bool HostRamLedger::take(uint64_t bytes)
{
    AV_SCOPE("HostRamLedger::take");
    if (!admit(bytes)) {
        __atomic_fetch_add(&tight_refusals_, 1u, __ATOMIC_RELAXED);
        refuse();
        return false;
    }
    if (ByteLedger::take(bytes)) return true;
    restore(bytes);
    return false;
}

bool HostRamLedger::take_session(uint64_t bytes)
{
    AV_SCOPE("HostRamLedger::take_session");
    return ByteLedger::take(bytes);
}

bool HostRamLedger::admit(uint64_t bytes)
{
    AV_SCOPE("HostRamLedger::admit");
    HostMemoryGauge *const gauge = __atomic_load_n(&gauge_, __ATOMIC_ACQUIRE);
    const uint64_t         lowest = __atomic_load_n(&floor_, __ATOMIC_ACQUIRE);
    if (gauge == nullptr || lowest == 0u) return true;
    const uint64_t now = gauge->now_ms();
    uint64_t       stamp = __atomic_load_n(&stamp_, __ATOMIC_RELAXED);
    if ((stamp == never || now - stamp >= refresh_ms) &&
        __atomic_compare_exchange_n(&stamp_, &stamp, now, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        refresh(now);
    uint64_t left = __atomic_load_n(&available_, __ATOMIC_RELAXED);
    for (;;) {
        if (left == never) return true;
        if (left < bytes || left - bytes < lowest) return false;
        if (__atomic_compare_exchange_n(&available_, &left, left - bytes, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            return true;
    }
}

void HostRamLedger::refresh(uint64_t now)
{
    AV_SCOPE("HostRamLedger::refresh");
    HostMemoryGauge *const gauge = __atomic_load_n(&gauge_, __ATOMIC_ACQUIRE);
    uint64_t               found = 0u;
    if (!gauge->available(found)) return;
    __atomic_store_n(&available_, found, __ATOMIC_RELAXED);
    const bool tight = found < __atomic_load_n(&floor_, __ATOMIC_ACQUIRE);
    const bool was = __atomic_exchange_n(&tight_, tight ? 1u : 0u, __ATOMIC_RELAXED) != 0u;
    if (!tight || was) return;
    const uint64_t last = __atomic_load_n(&relieved_, __ATOMIC_RELAXED);
    if (last != never && now - last < relieve_ms) return;
    __atomic_store_n(&relieved_, now, __ATOMIC_RELAXED);
    gauge->relieve();
}

void HostRamLedger::restore(uint64_t bytes)
{
    AV_SCOPE("HostRamLedger::restore");
    uint64_t left = __atomic_load_n(&available_, __ATOMIC_RELAXED);
    while (left != never) {
        const uint64_t back = bytes > never - 1u - left ? never - 1u : left + bytes;
        if (__atomic_compare_exchange_n(&available_, &left, back, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) return;
    }
}

}
