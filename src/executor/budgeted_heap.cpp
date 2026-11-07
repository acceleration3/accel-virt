#include "executor/budgeted_heap.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

BudgetedHeap::BudgetedHeap() : raw_(), account_(), limit_(UINT64_MAX), refusals_(0u)
{
    AV_SCOPE("BudgetedHeap::BudgetedHeap");
}

void BudgetedHeap::bind(ByteLedger *ledger, uint64_t limit)
{
    AV_SCOPE("BudgetedHeap::bind");
    account_.bind(ledger);
    limit_ = limit;
}

void *BudgetedHeap::allocate(size_t bytes, Pool pool)
{
    AV_SCOPE("BudgetedHeap::allocate");
    if (bytes > SIZE_MAX - header_bytes) return nullptr;
    const uint64_t total = bytes + header_bytes;
    if (account_.held() > limit_ || total > limit_ - account_.held() || !account_.take(total)) {
        __atomic_fetch_add(&refusals_, 1u, __ATOMIC_RELAXED);
        return nullptr;
    }
    void *const raw = raw_.allocate(total, pool);
    if (raw == nullptr) {
        account_.give(total);
        return nullptr;
    }
    __builtin_memcpy(raw, &total, sizeof total);
    return static_cast<uint8_t *>(raw) + header_bytes;
}

void BudgetedHeap::release(void *block)
{
    AV_SCOPE("BudgetedHeap::release");
    if (block == nullptr) return;
    void *const raw = static_cast<uint8_t *>(block) - header_bytes;
    uint64_t    total = 0u;
    __builtin_memcpy(&total, raw, sizeof total);
    raw_.release(raw);
    account_.give(total);
}

void BudgetedHeap::require_irql_at_most(Irql, const char *)
{
    AV_SCOPE("BudgetedHeap::require_irql_at_most");
}

uint64_t BudgetedHeap::live() const
{
    AV_SCOPE("BudgetedHeap::live");
    return raw_.live();
}

uint64_t BudgetedHeap::held() const
{
    AV_SCOPE("BudgetedHeap::held");
    return account_.held();
}

uint64_t BudgetedHeap::limit() const
{
    AV_SCOPE("BudgetedHeap::limit");
    return limit_;
}

uint64_t BudgetedHeap::refusals() const
{
    AV_SCOPE("BudgetedHeap::refusals");
    return __atomic_load_n(&refusals_, __ATOMIC_RELAXED);
}

}
