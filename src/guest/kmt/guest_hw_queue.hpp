/* What `D3DKMTCreateHwQueue` answers a process (feature 049): the queue it
 * submits to, the monitored fence the queue's progress is reported on - which
 * is what a wait waits on - and the read-only mapping of that fence's value,
 * which the process reads without a kernel call.
 *
 * The three are one answer and travel together, from the call that makes them
 * to the doorbell that hangs off the queue. */
#ifndef AV_GUEST_HW_QUEUE_HPP
#define AV_GUEST_HW_QUEUE_HPP

#include <stdint.h>

namespace av {

class GuestHwQueue {
public:
    GuestHwQueue();
    GuestHwQueue(uint32_t handle, uint32_t progress_fence, const uint64_t *progress);

    uint32_t handle() const;
    uint32_t progress_fence() const;
    /* What the driver has written there, 0 where the kernel mapped nothing. */
    uint64_t progress() const;
    bool     established() const;

private:
    uint32_t        handle_;
    uint32_t        progress_fence_;
    const uint64_t *progress_;
};

}  // namespace av

#endif  // AV_GUEST_HW_QUEUE_HPP
