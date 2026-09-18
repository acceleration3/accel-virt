#include "guest/kmt/guest_hw_queue.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestHwQueue::GuestHwQueue() : handle_(0u), progress_fence_(0u), progress_(nullptr)
{
    AV_SCOPE("GuestHwQueue::GuestHwQueue");
}

GuestHwQueue::GuestHwQueue(uint32_t handle, uint32_t progress_fence, const uint64_t *progress)
    : handle_(handle), progress_fence_(progress_fence), progress_(progress)
{
    AV_SCOPE("GuestHwQueue::GuestHwQueue");
}

uint32_t GuestHwQueue::handle() const
{
    AV_SCOPE("GuestHwQueue::handle");
    return handle_;
}

uint32_t GuestHwQueue::progress_fence() const
{
    AV_SCOPE("GuestHwQueue::progress_fence");
    return progress_fence_;
}

uint64_t GuestHwQueue::progress() const
{
    AV_SCOPE("GuestHwQueue::progress");
    return progress_ != nullptr ? *progress_ : 0u;
}

bool GuestHwQueue::established() const
{
    AV_SCOPE("GuestHwQueue::established");
    return handle_ != 0u;
}

}  // namespace av
