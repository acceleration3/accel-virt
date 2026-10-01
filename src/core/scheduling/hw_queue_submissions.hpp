/* What hardware queues have submitted and the device has not finished: each
 * submission's progress fence id, the address dxgkrnl reads that fence from,
 * and the av context and device fence it waits on where it carried a batch
 * (feature 049). Completion writes the id into the fence, oldest first, and
 * reports MONITORED_FENCE_SIGNALED once, after which VidSch reads every
 * queue's fence. A submission carrying no batch has nothing to wait for and is
 * written at the next completion, as it was before there was one to carry.
 * dxgkrnl submits only to a queue between CreateHwQueue and DestroyHwQueue
 * (DXGHWQUEUE::DestroyCoreState terminates the queue first), so the handle is
 * recorded, not looked up: nothing here may block at DISPATCH. */
#ifndef AV_HW_QUEUE_SUBMISSIONS_HPP
#define AV_HW_QUEUE_SUBMISSIONS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class InterruptNotification;
struct HwQueueSubmissionsProbe;

class HwQueueSubmissions {
public:
    static constexpr uint32_t capacity = 64u;
    /* The hardware-scheduled node, which is where every queue runs (feature
     * 049, research.md §16). */
    static constexpr uint32_t node = 1u;
    static constexpr uint32_t engine = 0u;

    explicit HwQueueSubmissions(Platform &platform);
    HwQueueSubmissions(const HwQueueSubmissions &) = delete;
    HwQueueSubmissions &operator=(const HwQueueSubmissions &) = delete;

    /* invalid_parameter for no queue, no fence or a full set. `context` and
     * `awaited` name the av context whose ring carried the batch and the
     * device fence its completion waits for; null and 0 for a submission that
     * carried none. */
    [[nodiscard]] Status submit(const void *queue, uint64_t fence_id, uint64_t *fence, const void *context,
                                uint64_t awaited);
    /* DISPATCH, from the DPC's serve: the device has finished everything up to
     * `completed` on `context`, so every submission of that context waiting
     * for no more than that is written. */
    void                 completed_through(const void *context, uint64_t completed);
    /* DISPATCH. Writes what waits for nothing and reports once when anything
     * has been written since the last report. */
    [[nodiscard]] Status complete(InterruptNotification &notification);
    /* DestroyHwQueue: nothing more is written to that queue's fence. */
    void                 forget(const void *queue);
    /* ResetHwEngine and ResetFromTimeout: in-flight work is dropped unwritten. */
    void                 abandon();
    uint32_t             in_flight();

    bool invariants_hold() const;

private:

    Platform  &platform_;
    SpinLock   lock_;
    /* Oldest first: each submission's queue, fence id and fence. */
    const void *queues_[capacity];
    uint64_t    fence_ids_[capacity];
    uint64_t   *fences_[capacity];
    /* The av context whose ring carries each submission, and the device fence
     * its completion waits for; null and 0 where it carried no batch. */
    const void *contexts_[capacity];
    uint64_t    awaited_[capacity];
    uint32_t    count_;
    /* Whether anything has been written since the last report: VidSch reads
     * every queue's fence when it is told, so one report covers them all. */
    bool        written_;

    friend struct HwQueueSubmissionsProbe;
};

}  // namespace av

#endif  // AV_HW_QUEUE_SUBMISSIONS_HPP
