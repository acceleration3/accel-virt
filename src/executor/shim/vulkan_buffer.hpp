/* A buffer over memory another party exported as a dma-buf, on one device
 * (VulkanDeviceTable): the guest's own pages, which the device made a dma-buf
 * of (BlobDmaBuf), written by the GPU in place - a staged frame copied into a
 * window's surface with no CPU between. */
#ifndef AV_VULKAN_BUFFER_HPP
#define AV_VULKAN_BUFFER_HPP

#include <vulkan/vulkan_core.h>

namespace av {

class VulkanDeviceTable;

class VulkanBuffer {
public:
    VulkanBuffer();
    ~VulkanBuffer();
    VulkanBuffer(const VulkanBuffer &) = delete;
    VulkanBuffer &operator=(const VulkanBuffer &) = delete;

    /* `size` bytes of the dma-buf `fd`, imported to be copied into. The fd is
     * this one's whatever happens: closed where the import does not take it. */
    [[nodiscard]] bool import_dma_buf(const VulkanDeviceTable &table, int fd, VkDeviceSize size);
    void               release();
    bool               made() const;
    VkBuffer           buffer() const;

private:
    const VulkanDeviceTable *table_;
    VkBuffer                 buffer_;
    VkDeviceMemory           memory_;
};

}  // namespace av

#endif  // AV_VULKAN_BUFFER_HPP
