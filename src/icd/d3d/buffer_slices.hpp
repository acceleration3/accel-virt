#ifndef AV_D3D_BUFFER_SLICES_HPP
#define AV_D3D_BUFFER_SLICES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/d3d/slice_block.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class PollClock;
class ThreadLock;
}

namespace av::icd {
class HostChannel;
}

namespace av::d3d {

class GpuTimeline;

class BufferSlices {
public:
    struct Slice {
        uint64_t offset_;
        uint64_t bytes_;
    };

    static constexpr uint64_t smallest = 256u;
    static constexpr uint64_t largest_small = uint64_t{64} * 1024u;
    static constexpr uint32_t small_classes = 9u;
    static constexpr uint64_t chunk_bytes = uint64_t{256} * 1024u;
    static constexpr uint64_t large_alignment = uint64_t{64} * 1024u;
    static constexpr uint32_t slot_shift = 26u;
    static constexpr uint64_t slot_bytes = uint64_t{1} << slot_shift;
    static constexpr uint64_t first_block_bytes = uint64_t{4} << 20u;
    static constexpr uint64_t largest_block_bytes = slot_bytes;
    static constexpr uint64_t ceiling_bytes = uint64_t{512} << 20u;
    static constexpr uint32_t slot_count = 16u;
    static constexpr uint64_t spare_lifetime_nanoseconds = uint64_t{10} * 1000000000u;

    BufferSlices(BlockSource &memory, icd::HostChannel &channel, const void *owner, GpuTimeline &timeline,
                 ThreadLock &lock, PollClock &clock);
    ~BufferSlices();
    BufferSlices(const BufferSlices &) = delete;
    BufferSlices &operator=(const BufferSlices &) = delete;

    [[nodiscard]] Status take_slice(uint64_t bytes, Slice &slice);
    void                 retire(const Slice &slice);
    void                 give_back(const Slice &slice);
    void                 trim();

    uint8_t *view(const Slice &slice) const;
    uint64_t memory(const Slice &slice) const;
    uint64_t offset_in_block(const Slice &slice) const;

    uint32_t retired() const;
    uint32_t free_in_class(uint64_t bytes) const;
    uint32_t open_blocks() const;
    uint64_t open_bytes() const;

    bool invariants_hold() const;

private:
    struct Retired {
        uint64_t offset_;
        uint64_t bytes_;
        uint64_t tag_;
    };

    static constexpr uint32_t no_class = ~0u;
    static constexpr uint32_t no_slot = ~0u;

    uint32_t             class_of(uint64_t bytes) const;
    uint64_t             class_bytes(uint32_t which) const;
    [[nodiscard]] Status take_once(uint64_t bytes, Slice &slice);
    [[nodiscard]] Status carve(uint32_t which);
    [[nodiscard]] Status place_in_blocks(uint64_t bytes, uint64_t &offset);
    [[nodiscard]] Status open_block(uint64_t bytes);
    uint64_t             next_block_bytes(uint64_t bytes) const;
    void                 close_block(uint32_t slot);
    void                 note_empty(uint32_t slot);
    bool                 has_spare(uint32_t except) const;
    [[nodiscard]] Status make_retired_room();
    void                 compact_retired();
    void                 reclaim();
    void                 free_slice(uint64_t offset, uint64_t bytes);
    bool                 wait_oldest();

    BlockSource                &memory_;
    icd::HostChannel           &channel_;
    const void                 *owner_;
    GpuTimeline                &timeline_;
    ThreadLock                 &lock_;
    PollClock                  &clock_;
    ObjectStorage<SliceBlock>   blocks_[slot_count];
    uint64_t                    total_bytes_;
    GrowableArray<uint64_t>     free_[small_classes];
    uint32_t                    carved_[small_classes];
    GrowableArray<Retired>      retired_;
    uint32_t                    retired_first_;
    uint32_t                    outstanding_;
    bool                        lost_;

    friend struct BufferSlicesProbe;
};

}

#endif
