/* How far the host GPU has got with what a session's Direct3D 12 queues were
 * given.
 *
 * Every execute on a queue is followed by a signal of that queue's own fence
 * with the session's next mark, so marks rise across queues in the order work
 * was executed. Everything marked at or below reached() has finished on the
 * GPU: a queue whose fence is behind its last signal holds the answer at its
 * completed value, which is a mark it certainly passed; a queue that caught up
 * holds nothing back. The ring's worker completes a batch only once reached()
 * passes the mark the batch left behind, which is what lets the guest's kernel
 * signal an application's fence without the guest waiting for the host. */
#ifndef AV_D3D12_QUEUE_PROGRESS_HPP
#define AV_D3D12_QUEUE_PROGRESS_HPP

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;

namespace av {

class D3d12FenceWake;
struct D3d12QueueProgressProbe;

class D3d12QueueProgress {
public:
    /* More queues than an application makes; past it an execute is refused. */
    static constexpr uint32_t capacity = 16u;

    explicit D3d12QueueProgress(D3d12FenceWake &wake);
    D3d12QueueProgress(const D3d12QueueProgress &) = delete;
    D3d12QueueProgress &operator=(const D3d12QueueProgress &) = delete;

    /* After an execute on `queue`, the guest's `id`: the queue signals its own
     * fence, made on `device` the first time, with the next mark, and the wake
     * watches for it. False when the fence cannot be made, signalled or
     * watched, or the table is full. */
    [[nodiscard]] bool executed(uint32_t id, ID3D12CommandQueue *queue, ID3D12Device *device);
    /* The queue is going: its fence goes with it. */
    void               forget(uint32_t id);
    /* Every fence goes; marks keep rising. */
    void               clear();

    /* The mark the latest execute left, 0 before the first. */
    uint64_t marked() const;
    /* The highest mark everything up to which has finished. */
    uint64_t reached() const;

    bool invariants_hold() const;

private:
    uint32_t slot_of(uint32_t id) const;

    D3d12FenceWake &wake_;
    uint32_t     ids_[capacity];
    ID3D12Fence *fences_[capacity];
    uint64_t     signalled_[capacity];
    uint32_t     count_;
    uint64_t     marked_;

    friend struct D3d12QueueProgressProbe;
};

}  // namespace av

#endif  // AV_D3D12_QUEUE_PROGRESS_HPP
