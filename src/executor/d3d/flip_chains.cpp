#include "executor/d3d/flip_chains.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

FlipChains::FlipChains() : chains_{}, used_(0u), rotations_(0u)
{
    AV_SCOPE("FlipChains::FlipChains");
}

uint32_t FlipChains::chain_of(uint64_t surface) const
{
    AV_SCOPE("FlipChains::chain_of");
    for (uint32_t chain = 0u; chain < used_; chain++)
        for (uint32_t index = 0u; index < chains_[chain].count_; index++)
            if (chains_[chain].members_[index] == surface) return chain;
    return none;
}

bool FlipChains::holds_exactly(uint32_t chain, const uint64_t *surfaces, uint32_t count) const
{
    AV_SCOPE("FlipChains::holds_exactly");
    if (chains_[chain].count_ != count) return false;
    for (uint32_t index = 0u; index < count; index++)
        if (chain_of(surfaces[index]) != chain) return false;
    return true;
}

void FlipChains::drop(uint32_t chain)
{
    AV_SCOPE("FlipChains::drop");
    chains_[chain] = chains_[--used_];
    chains_[used_] = Chain{};
}

void FlipChains::remove(uint64_t surface)
{
    AV_SCOPE("FlipChains::remove");
    const uint32_t found = chain_of(surface);
    if (found == none) return;
    Chain &chain = chains_[found];
    for (uint32_t index = 0u; index < chain.count_; index++) {
        if (chain.members_[index] != surface) continue;
        chain.members_[index] = chain.members_[--chain.count_];
        chain.members_[chain.count_] = 0u;
        break;
    }
    if (chain.count_ < 2u) drop(found);
}

uint32_t FlipChains::free_slot()
{
    AV_SCOPE("FlipChains::free_slot");
    if (used_ < chain_capacity) return used_++;
    uint32_t oldest = 0u;
    for (uint32_t chain = 1u; chain < used_; chain++)
        if (chains_[chain].rotated_at_ < chains_[oldest].rotated_at_) oldest = chain;
    chains_[oldest] = Chain{};
    return oldest;
}

void FlipChains::rotated(const uint64_t *surfaces, uint32_t count)
{
    AV_SCOPE("FlipChains::rotated");
    if (surfaces == nullptr || count < 2u || count > member_capacity) return;
    for (uint32_t index = 0u; index < count; index++) {
        if (surfaces[index] == 0u) return;
        for (uint32_t other = 0u; other < index; other++)
            if (surfaces[other] == surfaces[index]) return;
    }
    rotations_++;
    const uint32_t known = chain_of(surfaces[0]);
    if (known != none && holds_exactly(known, surfaces, count)) {
        chains_[known].rotated_at_ = rotations_;
        return;
    }
    for (uint32_t index = 0u; index < count; index++) remove(surfaces[index]);
    Chain &chain = chains_[free_slot()];
    chain.count_ = count;
    chain.rotated_at_ = rotations_;
    for (uint32_t index = 0u; index < count; index++) chain.members_[index] = surfaces[index];
}

uint32_t FlipChains::siblings(uint64_t surface, uint64_t *out, uint32_t room) const
{
    AV_SCOPE("FlipChains::siblings");
    const uint32_t chain = surface != 0u && out != nullptr ? chain_of(surface) : none;
    if (chain == none) return 0u;
    uint32_t written = 0u;
    for (uint32_t index = 0u; index < chains_[chain].count_ && written < room; index++)
        if (chains_[chain].members_[index] != surface) out[written++] = chains_[chain].members_[index];
    return written;
}

bool FlipChains::invariants_hold() const
{
    AV_SCOPE("FlipChains::invariants_hold");
    if (used_ > chain_capacity) return false;
    for (uint32_t chain = 0u; chain < used_; chain++) {
        const Chain &held = chains_[chain];
        if (held.count_ < 2u || held.count_ > member_capacity || held.rotated_at_ > rotations_) return false;
        for (uint32_t index = 0u; index < held.count_; index++)
            if (held.members_[index] == 0u || chain_of(held.members_[index]) != chain) return false;
    }
    return true;
}

}
