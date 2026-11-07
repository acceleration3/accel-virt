#ifndef AV_VRAM_POLICY_HPP
#define AV_VRAM_POLICY_HPP

#include <stdint.h>

namespace av {

class VramPolicy {
public:
    static constexpr uint64_t mebibyte = 1ull << 20;
    static constexpr uint64_t auto_floor = 1ull << 30;
    static constexpr uint64_t auto_divisor = 8u;
    static constexpr uint64_t least_reserve = 256ull << 20;
    static constexpr uint64_t least_left = 512ull << 20;

    VramPolicy();
    explicit VramPolicy(uint32_t reserve_mib);

    uint32_t reserve_mib() const;
    uint64_t reserve_for(uint64_t total) const;
    uint64_t cap_for(uint64_t total) const;

private:
    uint32_t reserve_mib_;
};

}

#endif
