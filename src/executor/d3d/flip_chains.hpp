#ifndef AV_FLIP_CHAINS_HPP
#define AV_FLIP_CHAINS_HPP

#include <stdint.h>

namespace av {

class FlipChains {
public:
    static constexpr uint32_t chain_capacity = 32u;
    static constexpr uint32_t member_capacity = 16u;

    FlipChains();
    FlipChains(const FlipChains &) = delete;
    FlipChains &operator=(const FlipChains &) = delete;

    void     rotated(const uint64_t *surfaces, uint32_t count);
    uint32_t siblings(uint64_t surface, uint64_t *out, uint32_t room) const;
    bool     invariants_hold() const;

private:
    struct Chain {
        uint64_t members_[member_capacity];
        uint32_t count_;
        uint64_t rotated_at_;
    };

    static constexpr uint32_t none = ~uint32_t{0};

    uint32_t chain_of(uint64_t surface) const;
    bool     holds_exactly(uint32_t chain, const uint64_t *surfaces, uint32_t count) const;
    void     remove(uint64_t surface);
    void     drop(uint32_t chain);
    uint32_t free_slot();

    Chain    chains_[chain_capacity];
    uint32_t used_;
    uint64_t rotations_;
};

}

#endif
