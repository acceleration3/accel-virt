/* The Direct3D 12 fences whose completed values the guest reads from memory.
 *
 * A guest application asks a fence how far it has got far more often than it
 * waits on one - every frame's GetCompletedValue, every SetEventOnCompletion's
 * check - and a round trip each time would be a frame's budget spent asking.
 * So a fence the guest publishes has its completed value written, as a u64,
 * at an offset in the session's blob arena whenever it moves, between batches
 * on the context's worker: the guest's driver reads that word. */
#ifndef AV_D3D12_FENCE_PUBLISHER_HPP
#define AV_D3D12_FENCE_PUBLISHER_HPP

#include <stdint.h>

struct ID3D12Fence;

namespace av {

class GuestRegion;

class D3d12FencePublisher {
public:
    static constexpr uint32_t capacity = 256u;
    static constexpr uint64_t stop = ~uint64_t{0};

    explicit D3d12FencePublisher(GuestRegion &region);

    /* `fence` (the guest's `id`) is written at `offset` from now on, starting
     * now; offset `stop` ends it. False for an offset outside the region, a
     * misaligned one, or a full table. The fence is not referenced: the
     * session forgets it here before releasing it. */
    [[nodiscard]] bool publish(uint32_t id, ID3D12Fence *fence, uint64_t offset);
    /* A queue will signal `id` to `value`: until it has, publishing is pending. */
    void               expect(uint32_t id, uint64_t value);
    void               forget(uint32_t id);
    /* Writes every published fence that moved; true while one has not yet
     * reached a value it is expected to. */
    bool               update();
    void               clear();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    uint32_t slot_of(uint32_t id) const;
    void     write(uint32_t slot, uint64_t value);

    GuestRegion &region_;
    uint32_t     ids_[capacity];
    ID3D12Fence *fences_[capacity];
    uint64_t     offsets_[capacity];
    uint64_t     written_[capacity];
    uint64_t     expected_[capacity];
    uint32_t     count_;
};

}  // namespace av

#endif  // AV_D3D12_FENCE_PUBLISHER_HPP
