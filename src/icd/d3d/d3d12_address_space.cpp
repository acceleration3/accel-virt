#include "icd/d3d/d3d12_address_space.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {

constexpr uint64_t end = uint64_t{1} << D3d12AddressSpace::address_bits;

}  // namespace

D3d12AddressSpace::D3d12AddressSpace() : next_(first)
{
    AV_SCOPE("D3d12AddressSpace::D3d12AddressSpace");
}

uint64_t D3d12AddressSpace::reserve(uint64_t bytes)
{
    AV_SCOPE("D3d12AddressSpace::reserve");
    if (bytes == 0u || bytes > end) return 0u;
    const uint64_t rounded = (bytes + (alignment - 1u)) & ~(alignment - 1u);
    uint64_t base = __atomic_load_n(&next_, __ATOMIC_RELAXED);
    do {
        if (end - base < rounded) return 0u;
    } while (!__atomic_compare_exchange_n(&next_, &base, base + rounded, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    return base;
}

uint64_t D3d12AddressSpace::reserved() const
{
    AV_SCOPE("D3d12AddressSpace::reserved");
    return __atomic_load_n(&next_, __ATOMIC_RELAXED) - first;
}

bool D3d12AddressSpace::invariants_hold() const
{
    AV_SCOPE("D3d12AddressSpace::invariants_hold");
    const uint64_t next = __atomic_load_n(&next_, __ATOMIC_RELAXED);
    return next >= first && next <= end && next % alignment == 0u;
}

}  // namespace av::d3d
