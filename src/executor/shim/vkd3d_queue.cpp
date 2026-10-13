#include "executor/shim/vkd3d_queue.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/vkd3d_interop.hpp"

namespace av {

Vkd3dQueue::Vkd3dQueue() : interop_(nullptr), d3d12_(nullptr), queue_(VK_NULL_HANDLE), family_(0u)
{
    AV_SCOPE("Vkd3dQueue::Vkd3dQueue");
}

void Vkd3dQueue::borrow(ID3D12DXVKInteropDevice *interop, ID3D12CommandQueue *queue)
{
    AV_SCOPE("Vkd3dQueue::borrow");
    interop_ = nullptr;
    d3d12_ = nullptr;
    queue_ = VK_NULL_HANDLE;
    family_ = 0u;
    UINT32 family = 0u;
    if (interop == nullptr || queue == nullptr || FAILED(interop->GetVulkanQueueInfo(queue, &queue_, &family))) {
        queue_ = VK_NULL_HANDLE;
        return;
    }
    interop_ = interop;
    d3d12_ = queue;
    family_ = family;
}

VkQueue Vkd3dQueue::queue() const
{
    AV_SCOPE("Vkd3dQueue::queue");
    return queue_;
}

uint32_t Vkd3dQueue::family() const
{
    AV_SCOPE("Vkd3dQueue::family");
    return family_;
}

void Vkd3dQueue::flush()
{
    AV_SCOPE("Vkd3dQueue::flush");
}

void Vkd3dQueue::lock()
{
    AV_SCOPE("Vkd3dQueue::lock");
    if (interop_ != nullptr) static_cast<void>(interop_->LockCommandQueue(d3d12_));
}

void Vkd3dQueue::unlock()
{
    AV_SCOPE("Vkd3dQueue::unlock");
    if (interop_ != nullptr) static_cast<void>(interop_->UnlockCommandQueue(d3d12_));
}

}  // namespace av
