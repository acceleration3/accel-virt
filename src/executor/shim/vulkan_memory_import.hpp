/* Memory of the GPU's memory source turned into a guest device's Vulkan
 * memory, and back; a guest's shared objects exported and imported
 * (docs/host-memory.md).
 *
 * The driver's half of VulkanBackend::adopt, over the table the backend loads
 * for the guest's device (VulkanDeviceTable): every import goes through its
 * one primitive. Everything that decides whether an import should happen -
 * whether a device exists, whether the type asked for is one the memory was
 * allocated as - stays in the backend where a test can judge it; what is here
 * is only the calls the driver answers. */
#ifndef AV_VULKAN_MEMORY_IMPORT_HPP
#define AV_VULKAN_MEMORY_IMPORT_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;
class VulkanDeviceTable;

class VulkanMemoryImport {
public:
    /* `table` is the guest device's, loaded and forgotten with it. */
    explicit VulkanMemoryImport(const VulkanDeviceTable &table);
    VulkanMemoryImport(const VulkanMemoryImport &) = delete;
    VulkanMemoryImport &operator=(const VulkanMemoryImport &) = delete;

    /* Whether the device was made with the dma-buf import extensions. */
    bool               imports() const;
    void               release(uint64_t memory);
    /* The memory types dma-buf `fd` may be imported into, or 0 where the
     * device was made without the dma-buf extension. */
    uint32_t           dmabuf_types(int fd) const;
    /* `size` bytes of the dma-buf `descriptor` imported. Vulkan takes the
     * descriptor on success; on failure it stays held. */
    [[nodiscard]] bool import_dmabuf(OwnedFd &descriptor, uint64_t size, uint32_t memory_type, uint64_t &memory);
    /* `type` when the driver says a buffer or an image made as `create` asks
     * may be bound to memory imported from a handle of that type, else 0:
     * a driver may refuse an optimal colour image or a sparse buffer over
     * imported memory. 0 with no physical device. */
    VkExternalMemoryHandleTypeFlags buffer_external_types(VkPhysicalDevice physical, const VkBufferCreateInfo &create,
                                                          VkExternalMemoryHandleTypeFlagBits type);
    VkExternalMemoryHandleTypeFlags image_external_types(VkPhysicalDevice physical, const VkImageCreateInfo &create,
                                                         VkExternalMemoryHandleTypeFlagBits type);

    /* A descriptor for `memory`, as `type` (feature 053), held in
     * `descriptor`: false where the device has no such entry point or the
     * driver refuses. */
    [[nodiscard]] bool export_memory(uint64_t memory, VkExternalMemoryHandleTypeFlagBits type, OwnedFd &descriptor);
    /* The same for a semaphore, always as an opaque fd: a dma-buf is memory
     * and a sync fd is a binary signal, and what a WDDM fence maps onto is a
     * timeline semaphore. */
    [[nodiscard]] bool export_semaphore(uint64_t semaphore, OwnedFd &descriptor);
    /* `size` bytes of `fd` imported as `type`, which must be one the driver
     * takes. A copy of `fd` is what is handed to Vulkan, so the caller's
     * stays the caller's whatever happens - a shared object's descriptor
     * outlives every import of it. */
    [[nodiscard]] bool import_shared_memory(int fd, VkExternalMemoryHandleTypeFlagBits type, uint64_t size,
                                            uint32_t memory_type, uint64_t &memory);
    /* A new timeline semaphore with `fd`'s payload imported into it, answered
     * in `semaphore`. A copy of `fd` is imported, for the same reason.
     * Nothing is left behind unless it answers true. */
    [[nodiscard]] bool import_shared_semaphore(int fd, uint64_t &semaphore);
    void               destroy_semaphore(uint64_t semaphore);

    /* Imports that succeeded and have not been released, which is what a test
     * or a teardown can ask about without a driver. */
    uint32_t held() const;

private:
    const VulkanDeviceTable &table_;
    uint32_t                 held_;
};

}  // namespace av

#endif  // AV_VULKAN_MEMORY_IMPORT_HPP
