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

#include "core/support/growable_array.hpp"

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;

namespace av {

class BlockSource;
class DriverLog;

class D3d12FenceWake;
struct D3d12QueueProgressProbe;

class D3d12QueueProgress {
public:
    D3d12QueueProgress(D3d12FenceWake &wake, BlockSource &blocks);
    D3d12QueueProgress(const D3d12QueueProgress &) = delete;
    D3d12QueueProgress &operator=(const D3d12QueueProgress &) = delete;

    /* After an execute on `queue`, the guest's `id`: the queue signals its own
     * fence, made on `device` the first time, with the next mark, and the wake
     * watches for it. False when the fence cannot be made, signalled or
     * watched, or the list of queues cannot grow. */
    [[nodiscard]] bool executed(uint32_t id, ID3D12CommandQueue *queue, ID3D12Device *device);
    /* Before an execute on `queue`, the guest's `id`: it waits on the GPU for
     * what every other queue was given. An application's Queue::Wait is
     * carried out by the guest's kernel on the queue's own context and never
     * reaches the host, so sibling queues are ordered by what was executed
     * first instead - never on a fence only the guest advances, which would
     * stall the host GPU until the guest's timeout. False when a wait is
     * refused. */
    [[nodiscard]] bool after_siblings(uint32_t id, ID3D12CommandQueue *queue) const;
    /* Every queue that executed signals its own fence with the next mark:
     * once reached() passes marked(), everything each one was given has
     * finished, a staged frame's copy behind its last execute included. One
     * fence signalled from several queues would be reached by whichever
     * queue got there first. False when a queue refuses its signal. */
    [[nodiscard]] bool mark_all();
    /* The queue is going: its fence goes with it. */
    void               forget(uint32_t id);
    /* Every fence goes; marks keep rising. */
    void               clear();
    /* Each queue's progress signalled and completed, to stderr: which queue
     * a drain that never finished is waiting for. */
    void               log_stuck(DriverLog &lines) const;

    /* The mark the latest execute left, 0 before the first. */
    uint64_t marked() const;
    /* The highest mark everything up to which has finished. */
    uint64_t reached() const;

    bool invariants_hold() const;

private:
    struct Queue {
        uint32_t            id_;
        /* The session's, which it releases only after forget(). */
        ID3D12CommandQueue *queue_;
        ID3D12Fence        *fence_;
        uint64_t            signalled_;
    };

    /* The index of `id`'s queue; size() for none. */
    uint32_t index_of(uint32_t id) const;

    D3d12FenceWake      &wake_;
    GrowableArray<Queue> queues_;
    uint64_t             marked_;

    friend struct D3d12QueueProgressProbe;
};

}  // namespace av

#endif  // AV_D3D12_QUEUE_PROGRESS_HPP
