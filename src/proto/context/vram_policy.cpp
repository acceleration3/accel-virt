#include "proto/context/vram_policy.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VramPolicy::VramPolicy() : reserve_mib_(0u)
{
    AV_SCOPE("VramPolicy::VramPolicy");
}

VramPolicy::VramPolicy(uint32_t reserve_mib) : reserve_mib_(reserve_mib)
{
    AV_SCOPE("VramPolicy::VramPolicy");
}

uint32_t VramPolicy::reserve_mib() const
{
    AV_SCOPE("VramPolicy::reserve_mib");
    return reserve_mib_;
}

uint64_t VramPolicy::reserve_for(uint64_t total) const
{
    AV_SCOPE("VramPolicy::reserve_for");
    if (reserve_mib_ == 0u) return total / auto_divisor > auto_floor ? total / auto_divisor : auto_floor;
    uint64_t reserve = static_cast<uint64_t>(reserve_mib_) * mebibyte;
    if (reserve < least_reserve) reserve = least_reserve;
    const uint64_t most = total > least_left ? total - least_left : 0u;
    return reserve < most ? reserve : most;
}

uint64_t VramPolicy::cap_for(uint64_t total) const
{
    AV_SCOPE("VramPolicy::cap_for");
    const uint64_t reserve = reserve_for(total);
    return total > reserve && total - reserve >= total / 2u ? total - reserve : total / 2u;
}

}
