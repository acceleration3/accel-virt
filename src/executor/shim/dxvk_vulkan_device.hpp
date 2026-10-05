/* DXVK's own Vulkan device, reached through its interop
 * (IDXGIVkInteropDevice1): images made on it over memory it allocates or
 * imports, handed to Direct3D 11 as textures, and waits put on its queue.
 *
 * One per host GPU, beside the Direct3D 11 device it is DXVK's: what makes the
 * scanout's dma-bufs (HostImageExport) and what shows a Direct3D 12 surface
 * (InteropTextures) both reach Vulkan through it. The entry points are the
 * loader's for DXVK's device, which the fd ones need: DXVK is built with them
 * enabled (tools/dxvk-patches). */
#ifndef AV_DXVK_VULKAN_DEVICE_HPP
#define AV_DXVK_VULKAN_DEVICE_HPP

#include <stdint.h>
#include <vulkan/vulkan.h>

struct ID3D11Device5;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC1;

namespace av {

class DxvkVulkanDevice {
public:
    DxvkVulkanDevice();
    ~DxvkVulkanDevice();
    DxvkVulkanDevice(const DxvkVulkanDevice &) = delete;
    DxvkVulkanDevice &operator=(const DxvkVulkanDevice &) = delete;

    /* Reaches `device`'s Vulkan device. False where DXVK offers no interop or
     * the device lacks the fd entry points; true again once connected. */
    [[nodiscard]] bool connect(ID3D11Device5 *device);
    bool               connected() const;
    void               release();

    [[nodiscard]] bool   create_image(const VkImageCreateInfo &info, VkImage &image) const;
    void                 destroy_image(VkImage image) const;
    VkMemoryRequirements requirements(VkImage image) const;
    /* The first memory type `allowed` permits, a device-local one first;
     * VK_MAX_MEMORY_TYPES for none. */
    uint32_t             memory_type(uint32_t allowed) const;
    /* Memory for `image` alone, `size` bytes of `type`, made or imported as
     * `chain` says (VkExportMemoryAllocateInfo, VkImportMemoryFdInfoKHR), and
     * bound to it. An imported fd is the memory's once this succeeds. */
    [[nodiscard]] bool   bind_memory(VkImage image, const void *chain, VkDeviceSize size, uint32_t type,
                                     VkDeviceMemory &memory) const;
    void                 free_memory(VkDeviceMemory memory) const;
    [[nodiscard]] bool   memory_fd(VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits type, int &fd) const;
    /* The first row of a linear image's only subresource. */
    VkSubresourceLayout  row_layout(VkImage image) const;
    /* `image` as a texture of DXVK's, described as `desc`; the caller owns the
     * reference, and keeps the image alive as long as the texture is. */
    [[nodiscard]] bool   wrap(const D3D11_TEXTURE2D_DESC1 &desc, VkImage image, ID3D11Texture2D *&texture) const;
    /* A timeline semaphore over the payload the OPAQUE_FD `fd` holds; the fd
     * is the semaphore's once this succeeds. */
    [[nodiscard]] bool   import_timeline(int fd, VkSemaphore &semaphore) const;
    void                 destroy_semaphore(VkSemaphore semaphore) const;
    /* DXVK's queue waits for `semaphore` to reach `value` before running what
     * is submitted to it after. */
    [[nodiscard]] bool   wait(VkSemaphore semaphore, uint64_t value) const;
    /* Everything DXVK has recorded is submitted and the GPU is done with it.
     * Under the Direct3D 11 device's lock: it flushes the immediate context. */
    void                 idle() const;
    /* A sync_file, the caller's, that signals once the GPU has finished
     * everything DXVK has recorded so far - nothing waits for it here. Under
     * the Direct3D 11 device's lock: it flushes the immediate context. False
     * where the device exports no sync_file. */
    [[nodiscard]] bool   sync_file(int &fd);

private:
    void                            *interop_;   /* IDXGIVkInteropDevice1, held */
    VkPhysicalDevice                 physical_;
    VkDevice                         device_;
    PFN_vkCreateImage                create_image_;
    PFN_vkDestroyImage               destroy_image_;
    PFN_vkGetImageMemoryRequirements requirements_;
    PFN_vkAllocateMemory             allocate_;
    PFN_vkFreeMemory                 free_;
    PFN_vkBindImageMemory            bind_;
    PFN_vkGetImageSubresourceLayout  layout_;
    PFN_vkGetMemoryFdKHR             memory_fd_;
    PFN_vkCreateSemaphore            create_semaphore_;
    PFN_vkDestroySemaphore           destroy_semaphore_;
    PFN_vkImportSemaphoreFdKHR       import_semaphore_;
    PFN_vkQueueSubmit                submit_;
    PFN_vkQueueWaitIdle              queue_idle_;
    PFN_vkGetSemaphoreFdKHR          semaphore_fd_;
    /* Signalled by the empty batch behind each sync_file and reset by its
     * export, so one serves every sync_file: a binary semaphore is unsignalled
     * again once a sync_file is exported from it, and destroying it would
     * wait for the batch that signals it, which is not this device's to wait
     * for. */
    VkSemaphore                      sync_;
    /* A signal submitted and never exported leaves the semaphore signalled,
     * which no batch may signal again: from then on there are no more. */
    bool                             sync_spent_;
};

}  // namespace av

#endif  // AV_DXVK_VULKAN_DEVICE_HPP
