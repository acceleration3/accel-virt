#include "icd/d3d/d3d12_commitment.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

D3d12Commitment::D3d12Commitment() : budget_(0u), committed_(0u)
{
    AV_SCOPE("D3d12Commitment::D3d12Commitment");
}

void D3d12Commitment::learn(uint64_t budget_bytes)
{
    AV_SCOPE("D3d12Commitment::learn");
    __atomic_store_n(&budget_, budget_bytes, __ATOMIC_RELAXED);
}

D3d12Commitment::Admission D3d12Commitment::charge(uint64_t bytes)
{
    AV_SCOPE("D3d12Commitment::charge");
    const uint64_t budget = __atomic_load_n(&budget_, __ATOMIC_RELAXED);
    uint64_t       before = __atomic_load_n(&committed_, __ATOMIC_RELAXED);
    uint64_t       after;
    do {
        if (budget != 0u && (bytes > budget || before > budget - bytes)) return Admission::refused;
        after = before + bytes;
    } while (!__atomic_compare_exchange_n(&committed_, &before, after, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    if (budget == 0u || bytes >= ask_from_bytes) return Admission::ask;
    return after > budget / near_denominator * near_numerator ? Admission::ask : Admission::defer;
}

void D3d12Commitment::release(uint64_t bytes)
{
    AV_SCOPE("D3d12Commitment::release");
    uint64_t before = __atomic_load_n(&committed_, __ATOMIC_RELAXED);
    uint64_t after;
    do {
        after = bytes > before ? 0u : before - bytes;
    } while (!__atomic_compare_exchange_n(&committed_, &before, after, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
}

uint64_t D3d12Commitment::committed() const
{
    AV_SCOPE("D3d12Commitment::committed");
    return __atomic_load_n(&committed_, __ATOMIC_RELAXED);
}

uint64_t D3d12Commitment::budget() const
{
    AV_SCOPE("D3d12Commitment::budget");
    return __atomic_load_n(&budget_, __ATOMIC_RELAXED);
}

bool D3d12Commitment::invariants_hold() const
{
    AV_SCOPE("D3d12Commitment::invariants_hold");
    const uint64_t budget = __atomic_load_n(&budget_, __ATOMIC_RELAXED);
    return budget == 0u || __atomic_load_n(&committed_, __ATOMIC_RELAXED) <= budget;
}

}
