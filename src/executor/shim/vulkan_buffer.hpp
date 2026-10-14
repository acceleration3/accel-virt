/* A buffer over memory another party exported as a dma-buf, on one device
 * (VulkanDeviceTable): a Direct3D 11 slice store's memory, which the GPU's
 * memory source allocated and the guest writes through the host window
 * (docs/host-memory.md), imported on DXVK's device for it to draw from in
 * place. */
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

    /* The dma-buf `fd` - memory of the GPU's memory source - as a buffer
     * of DXVK's to be drawn from in place (D3d11HostDevice::bind_storage):
     * every use a buffer has, and the device address DXVK takes of every
     * buffer. The fd is this one's whatever happens: closed where the import
     * does not take it. */
    [[nodiscard]] bool import_dma_buf_to_draw(const VulkanDeviceTable &table, int fd, VkDeviceSize size);
    void               release();
    bool               made() const;
    VkBuffer           buffer() const;

private:
    [[nodiscard]] bool import(const VulkanDeviceTable &table, int fd, VkDeviceSize size, VkBufferUsageFlags usage);

    const VulkanDeviceTable *table_;
    VkBuffer                 buffer_;
    VkDeviceMemory           memory_;
};

}  // namespace av

#endif  // AV_VULKAN_BUFFER_HPP
