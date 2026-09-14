#include "guest/suite/guest_session.hpp"

#include "guest/kmt/kmt.hpp"

namespace av {

GuestSession::GuestSession(Kmt &kmt, uint32_t adapter)
    : kmt_(kmt), adapter_(adapter), device_(0u), allocations_{}, views_{}, addresses_{}, pages_{},
      allocation_count_(0u), queue_(0u), fence_object_(0u), context_(0u), teardown_code_(0u) {}

/* A scenario reports its own teardown; a destructor has no one to tell. */
GuestSession::~GuestSession() { static_cast<void>(tear_down()); }

Status GuestSession::open_device()
{
    if (adapter_ == 0u || device_ != 0u) return Status::invalid_state;
    uint32_t     device = 0u;
    const Status status = accept(kmt_.create_device(adapter_, device), device);
    if (status == Status::ok) device_ = device;
    return status;
}

Status GuestSession::allocate(const uint8_t *descriptor, uint32_t size, uint32_t &index)
{
    if (device_ == 0u || allocation_count_ == max_allocations) return Status::invalid_state;
    uint32_t     allocation = 0u;
    const Status status = accept(kmt_.create_allocation(device_, descriptor, size, allocation), allocation);
    if (status != Status::ok) return status;
    index = allocation_count_;
    allocations_[allocation_count_++] = allocation;
    return Status::ok;
}

Status GuestSession::lock(uint32_t index, void *&view)
{
    view = nullptr;
    if (index >= allocation_count_ || views_[index] != nullptr) return Status::invalid_state;
    void        *locked = nullptr;
    const Status status = accept(kmt_.lock(device_, allocations_[index], locked), locked);
    if (status != Status::ok) return status;
    views_[index] = locked;
    view = locked;
    return Status::ok;
}

/* A view the driver would not take back is still held, so the teardown tries
 * again before the allocation under it is destroyed. */
Status GuestSession::unlock(uint32_t index)
{
    if (index >= allocation_count_ || views_[index] == nullptr) return Status::invalid_state;
    const Status status = kmt_.unlock(device_, allocations_[index]);
    if (status == Status::ok) views_[index] = nullptr;
    return status;
}

/* A queue answered without its fence object is still destroyed at teardown. */
Status GuestSession::open_paging_queue()
{
    if (device_ == 0u || queue_ != 0u) return Status::invalid_state;
    uint32_t     queue = 0u;
    uint32_t     fence_object = 0u;
    const Status status = accept(kmt_.create_paging_queue(device_, queue, fence_object), queue);
    if (status != Status::ok) return status;
    queue_ = queue;
    fence_object_ = fence_object;
    return accept(status, fence_object);
}

Status GuestSession::map(uint32_t index, uint64_t pages, uint64_t &address)
{
    address = 0u;
    if (index >= allocation_count_ || fence_object_ == 0u || addresses_[index] != 0u) return Status::invalid_state;
    uint64_t fence = 0u;
    Status   status = kmt_.make_resident(queue_, allocations_[index], fence);
    if (status == Status::ok) status = kmt_.wait_for_fence(device_, fence_object_, fence);
    uint64_t mapped = 0u;
    if (status == Status::ok)
        status = accept(kmt_.map_gpu_virtual_address(queue_, allocations_[index], pages, mapped, fence), mapped);
    if (status != Status::ok) return status;
    addresses_[index] = mapped;
    pages_[index] = pages;
    status = kmt_.wait_for_fence(device_, fence_object_, fence);
    if (status == Status::ok) address = mapped;
    return status;
}

Status GuestSession::open_context()
{
    if (device_ == 0u || context_ != 0u) return Status::invalid_state;
    uint32_t     context = 0u;
    const Status status = accept(kmt_.create_context(device_, context), context);
    if (status == Status::ok) context_ = context;
    return status;
}

Status GuestSession::submit(uint64_t commands, uint64_t length)
{
    if (context_ == 0u) return Status::invalid_state;
    return kmt_.submit(context_, commands, length, nullptr, 0u);
}

Status GuestSession::wait()
{
    if (context_ == 0u) return Status::invalid_state;
    return kmt_.wait_for_context(device_, context_);
}

Status GuestSession::await_vertical_blank(uint32_t source)
{
    if (device_ == 0u) return Status::invalid_state;
    return kmt_.wait_for_vertical_blank(adapter_, device_, source);
}

void GuestSession::note(Status &first, Status status)
{
    if (first != Status::ok || status == Status::ok) return;
    first = status;
    teardown_code_ = kmt_.last_code();
}

Status GuestSession::tear_down()
{
    Status first = Status::ok;
    teardown_code_ = 0u;
    if (context_ != 0u) note(first, kmt_.destroy_context(context_));
    context_ = 0u;
    for (uint32_t index = allocation_count_; index != 0u; index--) {
        if (addresses_[index - 1u] != 0u)
            note(first, kmt_.free_gpu_virtual_address(adapter_, addresses_[index - 1u], pages_[index - 1u]));
        addresses_[index - 1u] = 0u;
        pages_[index - 1u] = 0u;
    }
    if (queue_ != 0u) note(first, kmt_.destroy_paging_queue(queue_));
    queue_ = 0u;
    fence_object_ = 0u;
    for (uint32_t index = allocation_count_; index != 0u; index--) {
        if (views_[index - 1u] != nullptr) note(first, kmt_.unlock(device_, allocations_[index - 1u]));
        views_[index - 1u] = nullptr;
        note(first, kmt_.destroy_allocation(device_, allocations_[index - 1u]));
    }
    allocation_count_ = 0u;
    if (device_ != 0u) note(first, kmt_.destroy_device(device_));
    device_ = 0u;
    return first;
}

uint32_t GuestSession::teardown_code() const { return teardown_code_; }

/* Nothing outlives the device it was made on, a fence object its queue, or a
 * mapping or a view its allocation. */
bool GuestSession::invariants_hold() const
{
    bool mappings_held = true;
    for (uint32_t index = allocation_count_; index < max_allocations; index++)
        mappings_held = mappings_held && addresses_[index] == 0u && views_[index] == nullptr;
    const bool children = allocation_count_ != 0u || queue_ != 0u || context_ != 0u;
    return (device_ != 0u || !children) && (queue_ != 0u || fence_object_ == 0u) && mappings_held;
}

}  // namespace av
