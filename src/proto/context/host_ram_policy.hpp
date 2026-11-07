#ifndef AV_HOST_RAM_POLICY_HPP
#define AV_HOST_RAM_POLICY_HPP

#include <stdint.h>

namespace av {

class HostRamPolicy {
public:
    static constexpr uint64_t mebibyte = 1ull << 20;
    static constexpr uint64_t auto_floor = 4ull << 30;
    static constexpr uint64_t auto_divisor = 4u;
    static constexpr uint64_t least_reserve = 1ull << 30;
    static constexpr uint64_t least_budget = 1ull << 30;

    HostRamPolicy();
    explicit HostRamPolicy(uint32_t reserve_mib);

    uint32_t reserve_mib() const;
    uint64_t reserve_for(uint64_t host_total) const;
    uint64_t budget_for(uint64_t host_total, uint64_t guest_ram) const;

private:
    uint32_t reserve_mib_;
};

}

#endif
