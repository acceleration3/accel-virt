#include "executor/shim/dxvk_vulkan_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_interop.hpp"
#include "executor/shim/vulkan_buffer.hpp"
#include "executor/shim/vulkan_timeline.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <unistd.h>

namespace av {

DxvkVulkanDevice::DxvkVulkanDevice()
    : interop_(nullptr), sync_(VK_NULL_HANDLE), sync_spent_(false)
{
    AV_SCOPE("DxvkVulkanDevice::DxvkVulkanDevice");
}

DxvkVulkanDevice::~DxvkVulkanDevice()
{
    AV_SCOPE("DxvkVulkanDevice::~DxvkVulkanDevice");
    release();
}

bool DxvkVulkanDevice::connect(ID3D11Device5 *device)
{
    AV_SCOPE("DxvkVulkanDevice::connect");
    if (interop_ != nullptr) return true;
    IDXGIVkInteropDevice1 *interop = nullptr;
    if (device == nullptr ||
        FAILED(device->QueryInterface(__uuidof(IDXGIVkInteropDevice1), reinterpret_cast<void **>(&interop))))
        return false;
    VkInstance       instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice         vulkan = VK_NULL_HANDLE;
    interop->GetVulkanHandles(&instance, &physical, &vulkan);
    interop_ = interop;
    queue_.borrow(interop);
    if (table_.load(instance, physical, vulkan) && table_.interoperates() && work_.open(table_, queue_)) return true;
    release();
    return false;
}

bool DxvkVulkanDevice::connected() const
{
    AV_SCOPE("DxvkVulkanDevice::connected");
    return interop_ != nullptr && table_.loaded();
}

void DxvkVulkanDevice::release()
{
    AV_SCOPE("DxvkVulkanDevice::release");
    if (sync_ != VK_NULL_HANDLE) {
        work_.idle();
        table_.destroy_semaphore(sync_);
        sync_ = VK_NULL_HANDLE;
    }
    work_.close();
    sync_spent_ = false;
    table_.forget();
    queue_.borrow(nullptr);
    if (interop_ != nullptr) interop_->Release();
    interop_ = nullptr;
}

const VulkanDeviceTable &DxvkVulkanDevice::table() const
{
    AV_SCOPE("DxvkVulkanDevice::table");
    return table_;
}

bool DxvkVulkanDevice::wrap(const D3D11_TEXTURE2D_DESC1 &desc, VkImage image, ID3D11Texture2D *&texture) const
{
    AV_SCOPE("DxvkVulkanDevice::wrap");
    texture = nullptr;
    return interop_ != nullptr && SUCCEEDED(interop_->CreateTexture2DFromVkImage(&desc, image, &texture));
}

bool DxvkVulkanDevice::import_timeline(OwnedFd &descriptor, VulkanTimeline &timeline) const
{
    AV_SCOPE("DxvkVulkanDevice::import_timeline");
    return timeline.import_fd(table_, descriptor);
}

bool DxvkVulkanDevice::wait(const VulkanTimeline &timeline, uint64_t value)
{
    AV_SCOPE("DxvkVulkanDevice::wait");
    return work_.wait(timeline, value);
}

void DxvkVulkanDevice::idle()
{
    AV_SCOPE("DxvkVulkanDevice::idle");
    work_.idle();
}

bool DxvkVulkanDevice::signal(const VulkanTimeline &timeline, uint64_t value)
{
    AV_SCOPE("DxvkVulkanDevice::signal");
    return work_.signal(timeline, value);
}

bool DxvkVulkanDevice::sync_file(int &fd)
{
    AV_SCOPE("DxvkVulkanDevice::sync_file");
    fd = -1;
    if (!connected() || sync_spent_) return false;
    if (sync_ == VK_NULL_HANDLE) {
        const VkExportSemaphoreCreateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, nullptr,
                                                      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT};
        if (!table_.create_semaphore(&exported, sync_)) return false;
    }
    queue_.flush();
    const VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0u, nullptr, nullptr, 0u, nullptr, 1u, &sync_};
    queue_.lock();
    const bool submitted = table_.submit(queue_.queue(), submit);
    queue_.unlock();
    if (!submitted) return false;
    if (table_.semaphore_fd(sync_, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT, fd)) return true;
    sync_spent_ = true;
    return false;
}

}
