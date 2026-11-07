#include "executor/byte_ledger.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ByteLedger::ByteLedger() : cap_(UINT64_MAX), used_(0u), refusals_(0u)
{
    AV_SCOPE("ByteLedger::ByteLedger");
}

void ByteLedger::set_cap(uint64_t cap)
{
    AV_SCOPE("ByteLedger::set_cap");
    __atomic_store_n(&cap_, cap, __ATOMIC_RELEASE);
}

bool ByteLedger::take(uint64_t bytes)
{
    AV_SCOPE("ByteLedger::take");
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

void ByteLedger::give(uint64_t bytes)
{
    AV_SCOPE("ByteLedger::give");
    uint64_t used = __atomic_load_n(&used_, __ATOMIC_RELAXED);
    while (!__atomic_compare_exchange_n(&used_, &used, bytes > used ? 0u : used - bytes, true, __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED)) {
    }
}

uint64_t ByteLedger::cap() const
{
    AV_SCOPE("ByteLedger::cap");
    return __atomic_load_n(&cap_, __ATOMIC_ACQUIRE);
}

uint64_t ByteLedger::used() const
{
    AV_SCOPE("ByteLedger::used");
    return __atomic_load_n(&used_, __ATOMIC_ACQUIRE);
}

uint64_t ByteLedger::free_bytes() const
{
    AV_SCOPE("ByteLedger::free_bytes");
    const uint64_t cap = this->cap();
    const uint64_t used = this->used();
    return used < cap ? cap - used : 0u;
}

uint64_t ByteLedger::refusals() const
{
    AV_SCOPE("ByteLedger::refusals");
    return __atomic_load_n(&refusals_, __ATOMIC_RELAXED);
}

}
