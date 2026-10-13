#include "executor/shim/dxvk_vulkan_device.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/dxvk_interop.hpp"
#include "executor/d3d/frame_target.hpp"
#include "executor/shim/vulkan_buffer.hpp"
#include "executor/shim/vulkan_timeline.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <unistd.h>

namespace av {

DxvkVulkanDevice::DxvkVulkanDevice()
    : interop_(nullptr), sync_(VK_NULL_HANDLE), sync_spent_(false), written_(), writes_(0u)
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
    if (table_.load(instance, physical, vulkan) && work_.open(table_, queue_)) return true;
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
    written_.release();
    writes_ = 0u;
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

bool DxvkVulkanDevice::import_timeline(int fd, VulkanTimeline &timeline) const
{
    AV_SCOPE("DxvkVulkanDevice::import_timeline");
    return timeline.import_fd(table_, fd);
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

/* DXVK submits from a thread of its own, after its CS thread has recorded:
 * the flush drains the CS thread, and its submission lock waits for its
 * submission queue to empty, so the empty batch here is submitted after all
 * of it - and a signal covers everything earlier in submission order on the
 * queue. */
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

/* A same-layout transition makes DXVK run what it deferred on the image - a
 * clear, the copy that staged it - before the image is handed out. The
 * target's fd is the caller's: the import takes a copy of it. */
bool DxvkVulkanDevice::write(ID3D11Texture2D *texture, const FrameTarget &target, VulkanBuffer &buffer,
                             uint64_t &value)
{
    AV_SCOPE("DxvkVulkanDevice::write");
    value = 0u;
    if (!connected() || texture == nullptr || target.count() > PresentCopyRequest::max_regions) return false;
    if (!written_.made() && !written_.make_exportable(table_)) return false;
    IDXGIVkInteropSurface *surface = nullptr;
    if (FAILED(texture->QueryInterface(__uuidof(IDXGIVkInteropSurface), reinterpret_cast<void **>(&surface))))
        return false;
    VkImage           image = VK_NULL_HANDLE;
    VkImageLayout     layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    const bool        described = SUCCEEDED(surface->GetVulkanImageInfo(&image, &layout, &info));
    const VkImageSubresourceRange whole = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    if (described) interop_->TransitionSurfaceLayout(surface, &whole, layout, layout);
    surface->Release();
    if (!described || !buffer.import_dma_buf(table_, dup(target.fd()), target.bytes())) return false;
    VkBufferImageCopy regions[PresentCopyRequest::max_regions];
    for (uint32_t index = 0u; index < target.count(); index++) regions[index] = target.copy(index);
    const uint64_t next = writes_ + 1u;
    if (!work_.write(image, layout, buffer.buffer(), target.count(), regions, written_, next)) {
        buffer.release();
        return false;
    }
    writes_ = next;
    value = next;
    return true;
}

bool DxvkVulkanDevice::written(uint64_t value, uint64_t nanoseconds) const
{
    AV_SCOPE("DxvkVulkanDevice::written");
    return written_.wait(value, nanoseconds);
}

}  // namespace av
