#include "executor/vram_account.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/byte_ledger.hpp"

namespace av {

VramAccount::VramAccount() : ledger_(nullptr), held_(0u)
{
    AV_SCOPE("VramAccount::VramAccount");
}

VramAccount::VramAccount(ByteLedger &ledger) : ledger_(&ledger), held_(0u)
{
    AV_SCOPE("VramAccount::VramAccount");
}

void VramAccount::bind(ByteLedger *ledger)
{
    AV_SCOPE("VramAccount::bind");
    static_cast<void>(close());
    ledger_ = ledger;
}

bool VramAccount::bound() const
{
    AV_SCOPE("VramAccount::bound");
    return ledger_ != nullptr;
}

bool VramAccount::bound_to(const ByteLedger *ledger) const
{
    AV_SCOPE("VramAccount::bound_to");
    return ledger_ == ledger;
}

bool VramAccount::take(uint64_t bytes)
{
    AV_SCOPE("VramAccount::take");
    if (ledger_ != nullptr && !ledger_->take(bytes)) return false;
    __atomic_fetch_add(&held_, bytes, __ATOMIC_RELAXED);
    return true;
}

void VramAccount::give(uint64_t bytes)
{
    AV_SCOPE("VramAccount::give");
    uint64_t held = __atomic_load_n(&held_, __ATOMIC_RELAXED);
    uint64_t returned = 0u;
    do {
        returned = bytes < held ? bytes : held;
    } while (!__atomic_compare_exchange_n(&held_, &held, held - returned, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    if (ledger_ != nullptr) ledger_->give(returned);
}

uint64_t VramAccount::close()
{
    AV_SCOPE("VramAccount::close");
    const uint64_t left = __atomic_exchange_n(&held_, 0u, __ATOMIC_ACQ_REL);
    if (ledger_ != nullptr) ledger_->give(left);
    return left;
}

uint64_t VramAccount::held() const
{
    AV_SCOPE("VramAccount::held");
    return __atomic_load_n(&held_, __ATOMIC_ACQUIRE);
}

uint64_t VramAccount::cap() const
{
    AV_SCOPE("VramAccount::cap");
    return ledger_ != nullptr ? ledger_->cap() : UINT64_MAX;
}

uint64_t VramAccount::free_bytes() const
{
    AV_SCOPE("VramAccount::free_bytes");
    return ledger_ != nullptr ? ledger_->free_bytes() : UINT64_MAX;
}

uint64_t VramAccount::used() const
{
    AV_SCOPE("VramAccount::used");
    return ledger_ != nullptr ? ledger_->used() : 0u;
}

}
