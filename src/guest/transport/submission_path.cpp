#include "guest/transport/submission_path.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/transport_ring.hpp"

namespace av {

SubmissionPath::SubmissionPath(Kmt &kmt, GuestDoorbell &doorbell, TransportRing &ring)
    : kmt_(kmt), doorbell_(doorbell), ring_(ring), doorbell_submissions_(0u), kernel_submissions_(0u), publishes_tail_(false), user_submissions_(0u)
{
    AV_SCOPE("SubmissionPath::SubmissionPath");
}

Status SubmissionPath::submit(uint32_t context, uint64_t dma_address, const uint8_t *descriptor, uint32_t size,
                              uint64_t fence)
{
    AV_SCOPE("SubmissionPath::submit");
    if (doorbell_.established()) {
        /* The tail is published before the store, because the store is what
         * tells the device to read it. */
        const Status published = ring_.publish_tail();
        if (published != Status::ok) return published;
        if (doorbell_.ring(fence)) {
            doorbell_submissions_++;
            return Status::ok;
        }
        /* Refused: this submission goes through the kernel, and the doorbell
         * is put back the way the status asked - connected again, or made
         * anew - so the next one need not (US2 scenario 1). */
        static_cast<void>(doorbell_.recover());
    }
    /* Disconnected, never connected, or a machine that gives out no doorbell
     * at all: the same batch at the same tail, through the kernel - and where
     * there is a hardware queue, through that, because a hardware-scheduled
     * context refuses D3DKMTSubmitCommand outright (research.md §13). */
    if (publishes_tail_ && doorbell_.queue() == 0u) {
        const Status published = ring_.publish_tail();
        if (published != Status::ok) return published;
        /* The worker clears its flag before it looks at the tail a last time,
         * so seeing it set after the tail is stored means the batch is seen:
         * the kernel is needed only to wake a worker that is asleep. */
        if (ring_.consumer_awake()) {
            user_submissions_++;
            return Status::ok;
        }
    }
    const Status submitted =
        doorbell_.queue() != 0u
            ? kmt_.submit_to_hw_queue(doorbell_.queue(), dma_address, size, descriptor, size, fence)
            : kmt_.submit(context, dma_address, size, descriptor, size);
    if (submitted != Status::ok) return submitted;
    kernel_submissions_++;
    return Status::ok;
}

void SubmissionPath::publish_before_kernel()
{
    AV_SCOPE("SubmissionPath::publish_before_kernel");
    publishes_tail_ = true;
}

uint64_t SubmissionPath::through_doorbell() const
{
    AV_SCOPE("SubmissionPath::through_doorbell");
    return doorbell_submissions_;
}

uint64_t SubmissionPath::through_kernel() const
{
    AV_SCOPE("SubmissionPath::through_kernel");
    return kernel_submissions_;
}

bool SubmissionPath::invariants_hold() const
{
    AV_SCOPE("SubmissionPath::invariants_hold");
    /* Every submission counted is a batch this process wrote into its ring,
     * whichever way it went: nothing is counted for a ring nothing was
     * published into. */
    return (doorbell_submissions_ == 0u && kernel_submissions_ == 0u) || ring_.tail() != 0u;
}

}  // namespace av
