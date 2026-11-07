#include "proto/context/host_ram_policy.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostRamPolicy::HostRamPolicy() : reserve_mib_(0u)
{
    AV_SCOPE("HostRamPolicy::HostRamPolicy");
}

HostRamPolicy::HostRamPolicy(uint32_t reserve_mib) : reserve_mib_(reserve_mib)
{
    AV_SCOPE("HostRamPolicy::HostRamPolicy");
}

uint32_t HostRamPolicy::reserve_mib() const
{
    AV_SCOPE("HostRamPolicy::reserve_mib");
    return reserve_mib_;
}

uint64_t HostRamPolicy::reserve_for(uint64_t host_total) const
{
    AV_SCOPE("HostRamPolicy::reserve_for");
    if (reserve_mib_ != 0u) {
        const uint64_t asked = static_cast<uint64_t>(reserve_mib_) * mebibyte;
        return asked > least_reserve ? asked : least_reserve;
    }
    return host_total / auto_divisor > auto_floor ? host_total / auto_divisor : auto_floor;
}

uint64_t HostRamPolicy::floor_for(uint64_t host_total) const
{
    AV_SCOPE("HostRamPolicy::floor_for");
    if (host_total == 0u) return 0u;
    if (reserve_mib_ != 0u) return reserve_for(host_total);
    return host_total / floor_divisor > least_floor ? host_total / floor_divisor : least_floor;
}

uint64_t HostRamPolicy::budget_for(uint64_t host_total, uint64_t guest_ram) const
{
    AV_SCOPE("HostRamPolicy::budget_for");
    if (host_total == 0u) return 0u;
    const uint64_t taken = guest_ram > host_total ? host_total : guest_ram;
    const uint64_t reserve = reserve_for(host_total);
    const uint64_t left = host_total - taken;
    const uint64_t budget = left > reserve ? left - reserve : 0u;
    return budget > least_budget ? budget : least_budget;
}

}
