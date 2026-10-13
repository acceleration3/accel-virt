/* DXVK's Direct3D 11 Vulkan device, as the executor borrows it through its
 * interop (IDXGIVkInteropDevice1): the device's table (VulkanDeviceTable),
 * which images and timelines of the executor's are made on, its queue
 * (DxvkD3d11Queue) and the work the executor adds to it (QueueWork) - and
 * what only Direct3D 11 does: an image wrapped as a texture of DXVK's, and a
 * sync_file of everything DXVK has recorded.
 *
 * One per host GPU, beside the Direct3D 11 device it is DXVK's: what makes the
 * scanout's dma-bufs (HostImageExport) and what shows another API's surface
 * (ExportableImages) both reach Vulkan through it. The fd entry points are
 * there because DXVK is built with them enabled (tools/dxvk-patches). */
#ifndef AV_DXVK_VULKAN_DEVICE_HPP
#define AV_DXVK_VULKAN_DEVICE_HPP

#include "executor/shim/dxvk_queues.hpp"
#include "executor/shim/queue_work.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Device5;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class VulkanTimeline;

class DxvkVulkanDevice {
public:
    DxvkVulkanDevice();
    ~DxvkVulkanDevice();
    DxvkVulkanDevice(const DxvkVulkanDevice &) = delete;
    DxvkVulkanDevice &operator=(const DxvkVulkanDevice &) = delete;

    /* Reaches `device`'s Vulkan device. False where DXVK offers no interop or
     * the device lacks the fd entry points; true again once connected. */
    [[nodiscard]] bool       connect(ID3D11Device5 *device);
    bool                     connected() const;
    void                     release();
    const VulkanDeviceTable &table() const;

    /* `image` as a texture of DXVK's, described as `desc`; the caller owns the
     * reference, and keeps the image alive as long as the texture is. */
    [[nodiscard]] bool wrap(const D3D11_TEXTURE2D_DESC1 &desc, VkImage image, ID3D11Texture2D *&texture) const;
    /* `timeline` made over the OPAQUE_FD `fd` on DXVK's device; the fd is the
     * timeline's whatever happens. */
    [[nodiscard]] bool import_timeline(int fd, VulkanTimeline &timeline) const;
    /* DXVK's queue waits for `timeline` to reach `value` before running what
     * is submitted to it after. */
    [[nodiscard]] bool wait(const VulkanTimeline &timeline, uint64_t value);
    /* Everything DXVK has recorded is submitted and the GPU is done with it.
     * Under the Direct3D 11 device's lock: it flushes the immediate context. */
    void               idle();
    /* A sync_file, the caller's, that signals once the GPU has finished
     * everything DXVK has recorded so far - nothing waits for it here. Under
     * the Direct3D 11 device's lock: it flushes the immediate context. False
     * where the device exports no sync_file. */
    [[nodiscard]] bool sync_file(int &fd);

private:
    IDXGIVkInteropDevice1 *interop_;   /* held */
    VulkanDeviceTable      table_;
    DxvkD3d11Queue         queue_;
    QueueWork              work_;
    /* Signalled by the empty batch behind each sync_file and reset by its
     * export, so one serves every sync_file: a binary semaphore is unsignalled
     * again once a sync_file is exported from it, and destroying it would
     * wait for the batch that signals it, which is not this device's to wait
     * for. */
    VkSemaphore            sync_;
    /* A signal submitted and never exported leaves the semaphore signalled,
     * which no batch may signal again: from then on there are no more. */
    bool                   sync_spent_;
};

}  // namespace av

#endif  // AV_DXVK_VULKAN_DEVICE_HPP
