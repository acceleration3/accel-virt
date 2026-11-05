#include "executor/shim/d3d9_slice_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/vulkan_buffer.hpp"

namespace av {

D3d9SliceDevice::D3d9SliceDevice() : device_(nullptr), interop_(nullptr), table_(), queue_(VK_NULL_HANDLE)
{
    AV_SCOPE("D3d9SliceDevice::D3d9SliceDevice");
}

D3d9SliceDevice::~D3d9SliceDevice()
{
    AV_SCOPE("D3d9SliceDevice::~D3d9SliceDevice");
    release();
}

bool D3d9SliceDevice::connect(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9SliceDevice::connect");
    if (interop_ != nullptr) return device == device_;
    ID3D9VkInteropDevice1 *interop = nullptr;
    if (device == nullptr ||
        FAILED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice1), reinterpret_cast<void **>(&interop))))
        return false;
    VkInstance       instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice         vulkan = VK_NULL_HANDLE;
    interop->GetVulkanHandles(&instance, &physical, &vulkan);
    uint32_t index = 0u, family = 0u;
    interop->GetSubmissionQueue(&queue_, &index, &family);
    interop_ = interop;
    device_ = device;
    if (table_.load(instance, physical, vulkan) && table_.imports_dma_bufs()) return true;
    release();
    return false;
}

bool D3d9SliceDevice::connected() const
{
    AV_SCOPE("D3d9SliceDevice::connected");
    return interop_ != nullptr && table_.loaded();
}

bool D3d9SliceDevice::is(const IDirect3DDevice9 *device) const
{
    AV_SCOPE("D3d9SliceDevice::is");
    return device_ != nullptr && device_ == device;
}

void D3d9SliceDevice::release()
{
    AV_SCOPE("D3d9SliceDevice::release");
    if (interop_ == nullptr) return;
    idle();
    table_.forget();
    queue_ = VK_NULL_HANDLE;
    interop_->Release();
    interop_ = nullptr;
    device_ = nullptr;
}

void D3d9SliceDevice::idle()
{
    AV_SCOPE("D3d9SliceDevice::idle");
    if (interop_ == nullptr) return;
    interop_->FlushRenderingCommands();
    interop_->LockSubmissionQueue();
    table_.queue_idle(queue_);
    interop_->ReleaseSubmissionQueue();
}

bool D3d9SliceDevice::import_to_draw(OwnedFd &descriptor, uint64_t bytes, VulkanBuffer &buffer)
{
    AV_SCOPE("D3d9SliceDevice::import_to_draw");
    return connected() && buffer.import_dma_buf_to_draw(table_, descriptor, bytes);
}

bool D3d9SliceDevice::bind_storage(IDirect3DResource9 *buffer, const VulkanBuffer &storage, uint64_t offset,
                                   uint64_t bytes, void *host)
{
    AV_SCOPE("D3d9SliceDevice::bind_storage");
    if (!connected() || buffer == nullptr || !storage.made()) return false;
    return SUCCEEDED(interop_->BindBufferStorage(buffer, storage.buffer(), offset, bytes, host));
}

bool D3d9SliceDevice::invariants_hold() const
{
    AV_SCOPE("D3d9SliceDevice::invariants_hold");
    return (interop_ == nullptr) == (device_ == nullptr) && (interop_ == nullptr || table_.loaded());
}

}
