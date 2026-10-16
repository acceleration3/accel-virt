/* The slices of a session's slice store that buffers the CPU writes live in
 * (docs/mapped-buffers-plan.md, 3.1): host upload memory the session opened
 * for them (docs/host-memory.md), which the host GPU reads in place.
 *
 * A buffer an application maps for writing is a slice: memory the host GPU
 * reads in place, so what the application writes - whenever it writes it -
 * is what a draw reads when the GPU runs it. A map that asks for fresh
 * storage takes a new slice and retires the old one, which the draws already
 * recorded go on reading: it is tagged with the batch they go out in, and is
 * free again only once the host GPU has finished that batch (GpuTimeline).
 *
 * Small slices, up to largest_small, come in power-of-two classes from
 * blocks carved out of the store, each class a stack of free slices; a
 * renamed buffer is a pop and a push. Larger ones are placed first fit
 * (RangeTable) and given back whole. Retired slices wait in order of their
 * tags, which never decrease, so the oldest is the first to come back.
 *
 * Its bookkeeping - each class's free stack and the retired queue - grows
 * with the slices in use from the process's memory, apart from the store,
 * which is read through a write-combined mapping that reads uncached. Only
 * the store's bytes bound how many slices there are. Room in that
 * bookkeeping is made before a slice is handed out, so neither retiring nor
 * giving one back can fail.
 *
 * Every device of the session takes slices, from whichever thread the
 * application maps on, so every call is made under the lock this is given.
 * A call that must wait for the GPU waits holding it: a caller must not hold
 * the session's stream while it asks, since the wait may send it. */
#ifndef AV_D3D_BUFFER_SLICES_HPP
#define AV_D3D_BUFFER_SLICES_HPP

#include "core/support/status.hpp"
#include "core/support/growable_array.hpp"
#include "icd/common/range_table.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ThreadLock;
}  // namespace av

namespace av::d3d {

class GpuTimeline;

class BufferSlices {
public:
    /* A slice of the store; no bytes for none. */
    struct Slice {
        uint64_t offset_;
        uint64_t bytes_;
    };

    /* What a constant buffer's window is aligned to, and so every slice. */
    static constexpr uint64_t smallest = 256u;
    static constexpr uint64_t largest_small = uint64_t{64} * 1024u;
    static constexpr uint32_t small_classes = 9u;   /* 256 B to 64 KiB */
    /* What a small class carves from the store at a time. */
    static constexpr uint64_t block_bytes = uint64_t{256} * 1024u;
    /* A large slice is whole 64 KiB pages, which the host imports. */
    static constexpr uint64_t large_alignment = uint64_t{64} * 1024u;
    BufferSlices(BlockSource &memory, GpuTimeline &timeline, ThreadLock &lock);
    BufferSlices(const BufferSlices &) = delete;
    BufferSlices &operator=(const BufferSlices &) = delete;

    /* Takes slices from a store of `bytes`, rounded down to
     * large_alignment, from now on, forgetting every slice before. A store
     * too small for one large slice holds no slices. */
    void                 take(uint64_t bytes);
    /* A slice of at least `bytes`, waiting for retired ones to come back when
     * the arena has no room. invalid_parameter for no bytes; no_space when
     * none fits even with every retired slice back; no_memory when the
     * bookkeeping cannot grow; invalid_state when the timeline cannot be
     * waited on. */
    [[nodiscard]] Status take_slice(uint64_t bytes, Slice &slice);
    /* `slice` is no longer what a buffer is, but what is recorded so far may
     * still read it: it is free once the GPU finishes the batch being
     * recorded. */
    void                 retire(const Slice &slice);
    /* `slice` is free now: nothing was recorded that reads it. */
    void                 give_back(const Slice &slice);

    uint32_t retired() const;
    /* Small slices free in the class `bytes` falls in. */
    uint32_t free_in_class(uint64_t bytes) const;
    uint64_t begin() const;
    uint64_t end() const;

    /* Every free stack within what its class carved and inside the store,
     * the retired queue in order of tags with room for every slice handed
     * out, and the arena's ranges sound. */
    bool invariants_hold() const;

private:
    struct Retired {
        uint64_t offset_;
        uint64_t bytes_;
        uint64_t tag_;
    };

    static constexpr uint32_t no_class = ~0u;

    /* The small class of `bytes`, or no_class for a large slice. */
    uint32_t           class_of(uint64_t bytes) const;
    uint64_t           class_bytes(uint32_t which) const;
    [[nodiscard]] Status take_once(uint64_t bytes, Slice &slice);
    /* Carves a block for class `which`: no_space when the arena has none,
     * no_memory when its free stack cannot grow to hold it. */
    [[nodiscard]] Status carve(uint32_t which);
    /* Room in the retired queue for every slice handed out and one more. */
    [[nodiscard]] Status make_retired_room();
    /* The queue's reclaimed head dropped once it is half of it. */
    void               compact_retired();
    /* Every retired slice the GPU is done with, free. */
    void               reclaim();
    void               free_slice(uint64_t offset, uint64_t bytes);
    /* Waits for the oldest retired slice and frees what is done; false when
     * there is none or the wait failed. */
    bool               wait_oldest();

    GpuTimeline             &timeline_;
    ThreadLock              &lock_;
    icd::RangeTable          ranges_;
    GrowableArray<uint64_t>  free_[small_classes];
    uint32_t                 carved_[small_classes];
    /* Oldest first from retired_first_; those before it are reclaimed. */
    GrowableArray<Retired>   retired_;
    uint32_t                 retired_first_;
    /* Slices handed out and not yet retired or given back. */
    uint32_t                 outstanding_;
    bool                     lost_;

    friend struct BufferSlicesProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_BUFFER_SLICES_HPP
