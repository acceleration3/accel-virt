#include "guest/transport/submission_path.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/transport_ring.hpp"

namespace av {

SubmissionPath::SubmissionPath(Kmt &kmt, GuestDoorbell &doorbell, TransportRing &ring)
    : kmt_(kmt), doorbell_(doorbell), ring_(ring), doorbell_submissions_(0u), kernel_submissions_(0u)
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
    }
    /* Disconnected, never connected, or a machine that gives out no doorbell
     * at all: the same batch at the same tail, through the kernel. */
    const Status submitted = kmt_.submit(context, dma_address, size, descriptor, size);
    if (submitted != Status::ok) return submitted;
    kernel_submissions_++;
    return Status::ok;
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
    /* Counters only grow, and a doorbell submission is one this process could
     * have made: nothing is counted for a doorbell that was never set up. */
    return doorbell_submissions_ == 0u || doorbell_.handle() != 0u;
}

}  // namespace av
