#include "executor/vulkan/vulkan_parked_submits.hpp"

#include "core/platform/poll_clock.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_park_host.hpp"
#include "executor/vulkan/vulkan_submit_set.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

namespace {

bool shares(const GrowableArray<uint64_t> &first, const GrowableArray<uint64_t> &second)
{
    AV_SCOPE("shares");
    for (const uint64_t one : first)
        for (const uint64_t other : second)
            if (one == other) return true;
    return false;
}

}

VulkanParkedSubmits::Batch::Batch(BlockSource &blocks, const Request &request)
    : blocks_(blocks), events_(blocks, Pool::paged), waits_(blocks, Pool::paged), signals_(blocks, Pool::paged),
      frame_(nullptr), frame_bytes_(0u), queue_(request.queue_), device_(request.device_), mark_(request.mark_),
      since_(0u), command_(request.command_)
{
    AV_SCOPE("VulkanParkedSubmits::Batch::Batch");
}

VulkanParkedSubmits::Batch::~Batch()
{
    AV_SCOPE("VulkanParkedSubmits::Batch::~Batch");
    if (frame_ != nullptr) blocks_.release(frame_);
}

VulkanParkedSubmits::VulkanParkedSubmits(BlockSource &blocks, VulkanParkHost &host, const PollClock &clock)
    : blocks_(blocks), host_(host), clock_(&clock), pool_(blocks), order_(blocks, Pool::paged), serving_(false)
{
    AV_SCOPE("VulkanParkedSubmits::VulkanParkedSubmits");
}

VulkanParkedSubmits::~VulkanParkedSubmits()
{
    AV_SCOPE("VulkanParkedSubmits::~VulkanParkedSubmits");
    discard_silently();
}

void VulkanParkedSubmits::clock_from(const PollClock &clock)
{
    AV_SCOPE("VulkanParkedSubmits::clock_from");
    clock_ = &clock;
}

bool VulkanParkedSubmits::behind(uint64_t queue, const uint64_t *waits, uint32_t count) const
{
    AV_SCOPE("VulkanParkedSubmits::behind");
    for (const Batch *const batch : order_) {
        if (batch->queue_ == queue) return true;
        for (uint32_t index = 0u; index < count; index++)
            for (const uint64_t signal : batch->signals_)
                if (signal == waits[index]) return true;
    }
    return false;
}

Status VulkanParkedSubmits::fill(Batch &batch, const Request &request)
{
    AV_SCOPE("VulkanParkedSubmits::fill");
    WireReader   again = *request.frame_;
    const size_t bytes = again.remaining();
    batch.frame_ = static_cast<uint8_t *>(blocks_.allocate(bytes != 0u ? bytes : 1u, Pool::paged));
    if (batch.frame_ == nullptr) return Status::no_memory;
    batch.frame_bytes_ = bytes;
    if (!again.read(batch.frame_, bytes)) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < request.set_->event_count(); index++) {
        const Status kept = batch.events_.append(request.set_->events()[index]);
        if (kept != Status::ok) return kept;
    }
    for (uint32_t index = 0u; index < request.set_->wait_count(); index++) {
        const Status kept = batch.waits_.append(request.set_->waits()[index]);
        if (kept != Status::ok) return kept;
    }
    for (uint32_t index = 0u; index < request.set_->signal_count(); index++) {
        const Status kept = batch.signals_.append(request.set_->signals()[index]);
        if (kept != Status::ok) return kept;
    }
    return Status::ok;
}

Status VulkanParkedSubmits::park(const Request &request)
{
    AV_SCOPE("VulkanParkedSubmits::park");
    if (order_.size() >= most_parked) return Status::no_memory;
    Batch *batch = nullptr;
    if (pool_.create(batch, blocks_, request) != Status::ok) return Status::no_memory;
    batch->since_ = clock_->now_nanoseconds();
    Status kept = fill(*batch, request);
    if (kept == Status::ok) kept = order_.append(batch);
    if (kept != Status::ok) static_cast<void>(pool_.destroy(batch));
    return kept;
}

bool VulkanParkedSubmits::held_by_earlier(uint32_t index) const
{
    AV_SCOPE("VulkanParkedSubmits::held_by_earlier");
    const Batch &batch = *order_.at(index);
    for (uint32_t earlier = 0u; earlier < index; earlier++) {
        const Batch &ahead = *order_.at(earlier);
        if (ahead.queue_ == batch.queue_ || shares(ahead.signals_, batch.waits_)) return true;
    }
    return false;
}

bool VulkanParkedSubmits::events_unset(const Batch &batch) const
{
    AV_SCOPE("VulkanParkedSubmits::events_unset");
    for (const uint64_t event : batch.events_)
        if (host_.event_unset(batch.device_, event)) return true;
    return false;
}

void VulkanParkedSubmits::discard(uint32_t index)
{
    AV_SCOPE("VulkanParkedSubmits::discard");
    Batch *const batch = order_.at(index);
    order_.remove_at(index);
    host_.release_mark(batch->mark_);
    static_cast<void>(pool_.destroy(batch));
}

void VulkanParkedSubmits::discard_silently()
{
    AV_SCOPE("VulkanParkedSubmits::discard_silently");
    for (Batch *const batch : order_) static_cast<void>(pool_.destroy(batch));
    order_.clear();
}

void VulkanParkedSubmits::service()
{
    AV_SCOPE("VulkanParkedSubmits::service");
    if (serving_ || order_.size() == 0u) return;
    serving_ = true;
    const uint64_t now = clock_->now_nanoseconds();
    bool           failed = false;
    uint32_t       index = 0u;
    while (index < order_.size() && !failed) {
        const Batch &batch = *order_.at(index);
        if (held_by_earlier(index) || events_unset(batch)) {
            failed = now - batch.since_ >= park_nanoseconds;
            index++;
            continue;
        }
        const uint32_t command = batch.command_;
        const uint8_t *frame = batch.frame_;
        const size_t   bytes = batch.frame_bytes_;
        failed = !host_.replay(command, frame, bytes);
        discard(index);
    }
    if (failed) {
        host_.lose();
        drop_all();
    }
    serving_ = false;
}

void VulkanParkedSubmits::drop_event(uint64_t event)
{
    AV_SCOPE("VulkanParkedSubmits::drop_event");
    for (uint32_t index = order_.size(); index != 0u; index--) {
        const Batch &batch = *order_.at(index - 1u);
        for (const uint64_t waited : batch.events_) {
            if (waited != event) continue;
            discard(index - 1u);
            break;
        }
    }
}

void VulkanParkedSubmits::drop_device(uint64_t device)
{
    AV_SCOPE("VulkanParkedSubmits::drop_device");
    for (uint32_t index = order_.size(); index != 0u; index--)
        if (order_.at(index - 1u)->device_ == device) discard(index - 1u);
}

void VulkanParkedSubmits::drop_all()
{
    AV_SCOPE("VulkanParkedSubmits::drop_all");
    while (order_.size() != 0u) discard(order_.size() - 1u);
}

bool VulkanParkedSubmits::pending() const
{
    AV_SCOPE("VulkanParkedSubmits::pending");
    return order_.size() != 0u;
}

uint32_t VulkanParkedSubmits::count() const
{
    AV_SCOPE("VulkanParkedSubmits::count");
    return order_.size();
}

bool VulkanParkedSubmits::invariants_hold() const
{
    AV_SCOPE("VulkanParkedSubmits::invariants_hold");
    return order_.size() <= most_parked && pool_.live() == order_.size() && order_.invariants_hold() && pool_.invariants_hold();
}

}
