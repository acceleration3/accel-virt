#include "executor/shim/dxvk_queues.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_interop.hpp"

namespace av {

DxvkD3d11Queue::DxvkD3d11Queue() : interop_(nullptr), queue_(VK_NULL_HANDLE), family_(0u)
{
    AV_SCOPE("DxvkD3d11Queue::DxvkD3d11Queue");
}

void DxvkD3d11Queue::borrow(IDXGIVkInteropDevice1 *interop)
{
    AV_SCOPE("DxvkD3d11Queue::borrow");
    interop_ = interop;
    queue_ = VK_NULL_HANDLE;
    family_ = 0u;
    if (interop != nullptr) interop->GetSubmissionQueue(&queue_, &family_);
}

VkQueue DxvkD3d11Queue::queue() const
{
    AV_SCOPE("DxvkD3d11Queue::queue");
    return queue_;
}

uint32_t DxvkD3d11Queue::family() const
{
    AV_SCOPE("DxvkD3d11Queue::family");
    return family_;
}

void DxvkD3d11Queue::flush()
{
    AV_SCOPE("DxvkD3d11Queue::flush");
    if (interop_ != nullptr) interop_->FlushRenderingCommands();
}

void DxvkD3d11Queue::lock()
{
    AV_SCOPE("DxvkD3d11Queue::lock");
    if (interop_ != nullptr) interop_->LockSubmissionQueue();
}

void DxvkD3d11Queue::unlock()
{
    AV_SCOPE("DxvkD3d11Queue::unlock");
    if (interop_ != nullptr) interop_->ReleaseSubmissionQueue();
}

DxvkD3d9Queue::DxvkD3d9Queue() : interop_(nullptr), queue_(VK_NULL_HANDLE), family_(0u)
{
    AV_SCOPE("DxvkD3d9Queue::DxvkD3d9Queue");
}

void DxvkD3d9Queue::borrow(ID3D9VkInteropDevice *interop)
{
    AV_SCOPE("DxvkD3d9Queue::borrow");
    interop_ = interop;
    queue_ = VK_NULL_HANDLE;
    family_ = 0u;
    uint32_t index = 0u;
    if (interop != nullptr) interop->GetSubmissionQueue(&queue_, &index, &family_);
}

VkQueue DxvkD3d9Queue::queue() const
{
    AV_SCOPE("DxvkD3d9Queue::queue");
    return queue_;
}

uint32_t DxvkD3d9Queue::family() const
{
    AV_SCOPE("DxvkD3d9Queue::family");
    return family_;
}

void DxvkD3d9Queue::flush()
{
    AV_SCOPE("DxvkD3d9Queue::flush");
    if (interop_ != nullptr) interop_->FlushRenderingCommands();
}

void DxvkD3d9Queue::lock()
{
    AV_SCOPE("DxvkD3d9Queue::lock");
    if (interop_ != nullptr) interop_->LockSubmissionQueue();
}

void DxvkD3d9Queue::unlock()
{
    AV_SCOPE("DxvkD3d9Queue::unlock");
    if (interop_ != nullptr) interop_->ReleaseSubmissionQueue();
}

}
