#include "guest/transport/submission_path.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/transport/guest_doorbell.hpp"
#include "guest/transport/release_route.hpp"
#include "proto/escape/release_record.hpp"
#include "guest/transport/transport_ring.hpp"

namespace av {

SubmissionPath::SubmissionPath(Kmt &kmt, GuestDoorbell &doorbell, TransportRing &ring)
    : kmt_(kmt), doorbell_(doorbell), ring_(ring), doorbell_submissions_(0u), kernel_submissions_(0u), publishes_tail_(false), user_submissions_(0u), route_(nullptr), route_ring_(0u), release_submissions_(0u)
{
    AV_SCOPE("SubmissionPath::SubmissionPath");
}

Status SubmissionPath::submit(uint32_t context, uint64_t dma_address, const uint8_t *descriptor, uint32_t size,
                              uint64_t fence)
{
    AV_SCOPE("SubmissionPath::submit");
    if (route_ != nullptr) {
        SubmissionDescriptor batch;
        uint8_t              record[ReleaseRecord::encoded_size] = {};
        if (batch.decode(descriptor, size) != Status::ok) return Status::invalid_parameter;
        static_cast<void>(ReleaseRecord(route_ring_, batch).encode(record, sizeof record));
        const Status released = route_->release(record, sizeof record);
        if (released == Status::ok) {
            release_submissions_++;
            return Status::ok;
        }
        if (released != Status::not_supported) return released;
    }
    if (route_ == nullptr && doorbell_.established()) {
        const Status published = ring_.publish_tail();
        if (published != Status::ok) return published;
        if (doorbell_.ring(fence)) {
            doorbell_submissions_++;
            return Status::ok;
        }
        static_cast<void>(doorbell_.recover());
    }
    if (route_ == nullptr && publishes_tail_ && doorbell_.queue() == 0u) {
        const Status published = ring_.publish_tail();
        if (published != Status::ok) return published;
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

uint64_t SubmissionPath::submitted() const
{
    AV_SCOPE("SubmissionPath::submitted");
    return doorbell_submissions_ + kernel_submissions_ + user_submissions_ + release_submissions_;
}

void SubmissionPath::release_through(ReleaseRoute *route, uint32_t ring)
{
    AV_SCOPE("SubmissionPath::release_through");
    route_ = route;
    route_ring_ = route != nullptr ? ring : 0u;
}

bool SubmissionPath::releases() const
{
    AV_SCOPE("SubmissionPath::releases");
    return route_ != nullptr;
}

uint64_t SubmissionPath::through_release() const
{
    AV_SCOPE("SubmissionPath::through_release");
    return release_submissions_;
}

uint64_t SubmissionPath::through_kernel() const
{
    AV_SCOPE("SubmissionPath::through_kernel");
    return kernel_submissions_;
}

bool SubmissionPath::invariants_hold() const
{
    AV_SCOPE("SubmissionPath::invariants_hold");
    return (doorbell_submissions_ == 0u && kernel_submissions_ == 0u && release_submissions_ == 0u) || ring_.tail() != 0u;
}

}
