#include "icd/opencl/cl_arena_blocks.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClArenaBlocks::ClArenaBlocks() : base_(nullptr), bytes_(0u), offsets_(), sizes_(), count_(0u)
{
    AV_SCOPE("ClArenaBlocks::ClArenaBlocks");
}

void ClArenaBlocks::lend(uint8_t *base, uint64_t bytes)
{
    AV_SCOPE("ClArenaBlocks::lend");
    /* Every block of the old arena goes with it: memory that is gone must not
     * still be named by a row. */
    for (uint32_t slot = 0u; slot < max_blocks; ++slot) {
        offsets_[slot] = 0u;
        sizes_[slot] = 0u;
    }
    count_ = 0u;
    base_ = bytes == 0u ? nullptr : base;
    bytes_ = base_ == nullptr ? 0u : bytes;
}

bool ClArenaBlocks::ready() const
{
    AV_SCOPE("ClArenaBlocks::ready");
    return base_ != nullptr;
}

uint64_t ClArenaBlocks::whole_pages(uint64_t bytes) const
{
    AV_SCOPE("ClArenaBlocks::whole_pages");
    if (bytes > ~uint64_t{0} - (alignment - 1u)) return 0u;
    return (bytes + alignment - 1u) & ~(alignment - 1u);
}

uint64_t ClArenaBlocks::gap_for(uint64_t want) const
{
    AV_SCOPE("ClArenaBlocks::gap_for");
    uint64_t candidate = 0u;
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        /* Rows are in offset order, so what lies before this one is a gap. */
        if (offsets_[slot] - candidate >= want) return candidate;
        candidate = offsets_[slot] + sizes_[slot];
    }
    /* And whatever is left after the last row. */
    if (bytes_ - candidate >= want) return candidate;
    return bytes_;
}

bool ClArenaBlocks::take(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("ClArenaBlocks::take");
    if (base_ == nullptr || bytes == 0u || count_ == max_blocks) return false;
    const uint64_t want = whole_pages(bytes);
    if (want == 0u || want > bytes_) return false;
    const uint64_t at = gap_for(want);
    if (at == bytes_) return false;
    /* Kept in offset order, which is what makes a gap the space between two
     * rows rather than a search. */
    uint32_t slot = count_;
    while (slot > 0u && offsets_[slot - 1u] > at) {
        offsets_[slot] = offsets_[slot - 1u];
        sizes_[slot] = sizes_[slot - 1u];
        --slot;
    }
    offsets_[slot] = at;
    sizes_[slot] = want;
    ++count_;
    offset = at;
    return true;
}

void ClArenaBlocks::give_back(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::give_back");
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        if (offsets_[slot] != offset) continue;
        for (uint32_t next = slot + 1u; next < count_; ++next) {
            offsets_[next - 1u] = offsets_[next];
            sizes_[next - 1u] = sizes_[next];
        }
        --count_;
        offsets_[count_] = 0u;
        sizes_[count_] = 0u;
        return;
    }
}

uint8_t *ClArenaBlocks::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("ClArenaBlocks::at");
    if (base_ == nullptr || bytes == 0u) return nullptr;
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        if (offsets_[slot] != offset) continue;
        if (bytes > sizes_[slot]) return nullptr;
        return base_ + offset;
    }
    return nullptr;
}

uint32_t ClArenaBlocks::live() const
{
    AV_SCOPE("ClArenaBlocks::live");
    return count_;
}

bool ClArenaBlocks::invariants_hold() const
{
    AV_SCOPE("ClArenaBlocks::invariants_hold");
    if (count_ > max_blocks) return false;
    if (base_ == nullptr && (bytes_ != 0u || count_ != 0u)) return false;
    uint64_t previous_end = 0u;
    for (uint32_t slot = 0u; slot < count_; ++slot) {
        if (sizes_[slot] == 0u) return false;
        /* In order, and so never overlapping: two blocks over one span of
         * memory would be two buffers writing each other's bytes. */
        if (offsets_[slot] < previous_end) return false;
        if (sizes_[slot] > bytes_ - offsets_[slot]) return false;
        if ((offsets_[slot] % alignment) != 0u || (sizes_[slot] % alignment) != 0u) return false;
        previous_end = offsets_[slot] + sizes_[slot];
    }
    for (uint32_t slot = count_; slot < max_blocks; ++slot) {
        if (offsets_[slot] != 0u || sizes_[slot] != 0u) return false;
    }
    return true;
}

}  // namespace av::clicd
