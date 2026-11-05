#ifndef AV_D3D_SLICE_BLOCKS_HPP
#define AV_D3D_SLICE_BLOCKS_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/d3d/slice_block.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class DriverLog;
class HostAllocation;
class PollClock;
}

namespace av::icd {
class HostChannel;
}

namespace av::d3d {

class SliceBlocks {
public:
    static constexpr uint64_t large_alignment = uint64_t{64} * 1024u;
    static constexpr uint32_t slot_shift = 26u;
    static constexpr uint64_t slot_bytes = uint64_t{1} << slot_shift;
    static constexpr uint64_t first_block_bytes = uint64_t{4} << 20u;
    static constexpr uint64_t largest_block_bytes = slot_bytes;
    static constexpr uint64_t ceiling_bytes = uint64_t{512} << 20u;
    static constexpr uint32_t slot_count = 16u;
    static constexpr uint64_t spare_lifetime_nanoseconds = uint64_t{10} * 1000000000u;
    static constexpr uint64_t extra_lifetime_nanoseconds = uint64_t{1} * 1000000000u;
    static constexpr uint64_t reopen_pause_nanoseconds = uint64_t{1} * 1000000000u;

    SliceBlocks(BlockSource &memory, icd::HostChannel &channel, const void *owner, PollClock &clock,
                DriverLog &lines);
    ~SliceBlocks();
    SliceBlocks(const SliceBlocks &) = delete;
    SliceBlocks &operator=(const SliceBlocks &) = delete;

    [[nodiscard]] Status place(uint64_t bytes, uint64_t &offset);
    [[nodiscard]] Status open(uint64_t bytes, uint32_t &slot);
    void                 took(uint64_t offset);
    void                 remove(uint64_t offset);
    void                 freed(uint64_t offset);
    bool                 next_to_close(uint32_t &slot) const;
    void                 close(uint32_t slot);
    bool                 close_unused(uint32_t slot);

    uint32_t slot_of(uint64_t offset) const;
    uint64_t within(uint64_t offset) const;
    bool     holds(uint32_t slot) const;
    uint64_t bytes_of(uint32_t slot) const;
    uint8_t *view(uint64_t offset) const;
    uint64_t handle(uint64_t offset) const;
    uint32_t open_count() const;
    uint64_t open_bytes() const;

    bool invariants_hold() const;

private:
    uint64_t next_block_bytes(uint64_t bytes) const;
    bool     lifetime_over(uint32_t slot, uint32_t spare, uint64_t now) const;
    uint32_t newest_empty() const;
    void     release_memory(HostAllocation &allocation);

    static constexpr uint32_t none = ~0u;

    BlockSource               &memory_;
    icd::HostChannel          &channel_;
    const void                *owner_;
    PollClock                 &clock_;
    DriverLog                 &lines_;
    ObjectStorage<SliceBlock>  blocks_[slot_count];
    uint64_t                   total_bytes_;
    uint64_t                   reopen_at_;
    bool                       close_reported_;
};

}

#endif
