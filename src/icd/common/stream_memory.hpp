#ifndef AV_ICD_STREAM_MEMORY_HPP
#define AV_ICD_STREAM_MEMORY_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "icd/common/slice_blocks.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class DriverLog;
class PollClock;
class ThreadLock;
}

namespace av::icd {

class GpuTimeline;
class HostChannel;

class StreamMemory {
public:
    struct Slice {
        uint64_t offset_;
        uint64_t bytes_;
    };

    static constexpr uint64_t smallest = 256u;
    static constexpr uint64_t largest_small = uint64_t{64} * 1024u;
    static constexpr uint32_t most_small_classes = 9u;
    static constexpr uint64_t chunk_bytes = uint64_t{256} * 1024u;
    static constexpr uint64_t large_alignment = SliceBlocks::large_alignment;

    StreamMemory(BlockSource &memory, HostChannel &channel, const void *owner, GpuTimeline &timeline,
                 ThreadLock &lock, PollClock &clock, DriverLog &lines, HostMemoryRequest::Kind kind,
                 uint64_t alignment);
    ~StreamMemory();
    StreamMemory(const StreamMemory &) = delete;
    StreamMemory &operator=(const StreamMemory &) = delete;

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

    uint32_t             class_of(uint64_t bytes) const;
    uint64_t             class_bytes(uint32_t which) const;
    [[nodiscard]] Status take_once(uint64_t bytes, Slice &slice);
    [[nodiscard]] Status carve(uint32_t which);
    void                 forget_free_in(uint32_t slot);
    [[nodiscard]] Status make_retired_room();
    void                 compact_retired();
    void                 reclaim();
    void                 free_slice(uint64_t offset, uint64_t bytes);
    bool                 wait_oldest();

    BlockSource                &memory_;
    HostChannel           &channel_;
    const void                 *owner_;
    GpuTimeline                &timeline_;
    ThreadLock                 &lock_;
    SliceBlocks                 blocks_;
    uint64_t                    alignment_;
    uint32_t                    small_classes_;
    GrowableArray<uint64_t>     free_[most_small_classes];
    uint32_t                    carved_[most_small_classes];
    GrowableArray<Retired>      retired_;
    uint32_t                    retired_first_;
    uint32_t                    outstanding_;
    bool                        lost_;

    friend struct StreamMemoryProbe;
};

}

#endif
