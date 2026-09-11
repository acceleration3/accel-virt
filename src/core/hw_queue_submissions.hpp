/* What hardware queues have submitted and the device has not finished: each
 * submission's progress fence id and the address dxgkrnl reads that fence
 * from. Completion writes the id into the fence, oldest first, and reports
 * MONITORED_FENCE_SIGNALED once, after which VidSch reads every queue's fence.
 * dxgkrnl submits only to a queue between CreateHwQueue and DestroyHwQueue
 * (DXGHWQUEUE::DestroyCoreState terminates the queue first), so the handle is
 * recorded, not looked up: nothing here may block at DISPATCH. */
#ifndef AV_HW_QUEUE_SUBMISSIONS_HPP
#define AV_HW_QUEUE_SUBMISSIONS_HPP

#include "platform.hpp"
#include "spin_lock.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class InterruptNotification;
struct HwQueueSubmissionsProbe;

class HwQueueSubmissions {
public:
    static constexpr uint32_t capacity = 64u;

    explicit HwQueueSubmissions(Platform &platform);
    HwQueueSubmissions(const HwQueueSubmissions &) = delete;
    HwQueueSubmissions &operator=(const HwQueueSubmissions &) = delete;

    /* invalid_parameter for no queue, no fence or a full set. */
    [[nodiscard]] Status submit(const void *queue, uint64_t fence_id, uint64_t *fence);
    [[nodiscard]] Status complete(InterruptNotification &notification);
    /* DestroyHwQueue: nothing more is written to that queue's fence. */
    void                 forget(const void *queue);
    /* ResetHwEngine and ResetFromTimeout: in-flight work is dropped unwritten. */
    void                 abandon();
    uint32_t             in_flight();

    bool invariants_hold() const;

private:
    static constexpr uint32_t node = 0u;
    static constexpr uint32_t engine = 0u;

    Platform  &platform_;
    SpinLock   lock_;
    /* Oldest first: each submission's queue, fence id and fence. */
    const void *queues_[capacity];
    uint64_t    fence_ids_[capacity];
    uint64_t   *fences_[capacity];
    uint32_t    count_;

    friend struct HwQueueSubmissionsProbe;
};

}  // namespace av

#endif  // AV_HW_QUEUE_SUBMISSIONS_HPP
