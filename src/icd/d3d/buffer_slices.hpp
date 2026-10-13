/* The slices of a session's arena that buffers the CPU writes live in
 * (docs/mapped-buffers-plan.md, 3.1).
 *
 * A buffer an application maps for writing is a slice: memory the host GPU
 * reads in place, so what the application writes - whenever it writes it -
 * is what a draw reads when the GPU runs it. A map that asks for fresh
 * storage takes a new slice and retires the old one, which the draws already
 * recorded go on reading: it is tagged with the batch they go out in, and is
 * free again only once the host GPU has finished that batch (GpuTimeline).
 *
 * Small slices, up to largest_small, come in power-of-two classes from
 * blocks carved out of the arena, each class a stack of free slices; a
 * renamed buffer is a pop and a push. Larger ones are placed first fit
 * (RangeTable) and given back whole. Retired slices wait in order of their
 * tags, which never decrease, so the oldest is the first to come back.
 *
 * Every device of the session takes slices, from whichever thread the
 * application maps on, so every call is made under the lock this is given.
 * A call that must wait for the GPU waits holding it: a caller must not hold
 * the session's stream while it asks, since the wait may send it. */
#ifndef AV_D3D_BUFFER_SLICES_HPP
#define AV_D3D_BUFFER_SLICES_HPP

#include "core/support/status.hpp"
#include "icd/common/range_table.hpp"

#include <stdint.h>

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::d3d {

class GpuTimeline;

class BufferSlices {
public:
    /* A slice of the arena; no bytes for none. */
    struct Slice {
        uint64_t offset_;
        uint64_t bytes_;
    };

    /* What a constant buffer's window is aligned to, and so every slice. */
    static constexpr uint64_t smallest = 256u;
    static constexpr uint64_t largest_small = uint64_t{64} * 1024u;
    static constexpr uint32_t small_classes = 9u;   /* 256 B to 64 KiB */
    /* What a small class carves from the arena at a time. */
    static constexpr uint64_t block_bytes = uint64_t{256} * 1024u;
    /* A large slice is whole 64 KiB pages, which the host imports. */
    static constexpr uint64_t large_alignment = uint64_t{64} * 1024u;
    /* Slices of one small class that may exist, free or not: a class past it
     * waits for its own to come back rather than carve more. A game renames
     * a constant buffer a draw, thousands a frame, while the GPU runs a frame
     * behind: fewer than that in flight would stall it every frame. */
    static constexpr uint32_t max_per_class = 16384u;
    static constexpr uint32_t max_retired = 32768u;
    static constexpr uint32_t max_ranges = 4096u;

    BufferSlices(GpuTimeline &timeline, icd::ThreadLock &lock);
    BufferSlices(const BufferSlices &) = delete;
    BufferSlices &operator=(const BufferSlices &) = delete;

    /* Takes slices from [begin, end) of the arena from now on, forgetting
     * every slice before; both rounded inward to large_alignment. */
    void                 take(uint64_t begin, uint64_t end);
    /* A slice of at least `bytes`, waiting for retired ones to come back when
     * the arena has no room. invalid_parameter for no bytes; no_space when
     * none fits even with every retired slice back; invalid_state when the
     * timeline cannot be waited on. */
    [[nodiscard]] Status take_slice(uint64_t bytes, Slice &slice);
    /* `slice` is no longer what a buffer is, but what is recorded so far may
     * still read it: it is free once the GPU finishes the batch being
     * recorded. A full queue of retired slices first waits for its oldest. */
    void                 retire(const Slice &slice);
    /* `slice` is free now: nothing was recorded that reads it. */
    void                 give_back(const Slice &slice);

    uint32_t retired() const;
    /* Small slices free in the class `bytes` falls in. */
    uint32_t free_in_class(uint64_t bytes) const;
    uint64_t begin() const;
    uint64_t end() const;

    /* Every free and carved count within its class's bound, the retired
     * queue within its own and in order of tags, and the arena's ranges
     * sound. */
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
    /* Carves a block for class `which`: no_space when the arena has none or
     * the class is at max_per_class. */
    [[nodiscard]] Status carve(uint32_t which);
    /* Every retired slice the GPU is done with, free. */
    void               reclaim();
    void               free_slice(uint64_t offset, uint64_t bytes);
    /* Waits for the oldest retired slice and frees what is done; false when
     * there is none or the wait failed. */
    bool               wait_oldest();

    GpuTimeline                  &timeline_;
    icd::ThreadLock              &lock_;
    icd::RangeTable<max_ranges>   ranges_;
    uint64_t                      free_[small_classes][max_per_class];
    uint32_t                      free_count_[small_classes];
    uint32_t                      carved_[small_classes];
    Retired                       retired_[max_retired];
    uint32_t                      retired_first_;
    uint32_t                      retired_count_;
    bool                          lost_;

    friend struct BufferSlicesProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_BUFFER_SLICES_HPP
