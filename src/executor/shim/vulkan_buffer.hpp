/* A buffer over memory another party exported as a dma-buf, on one device
 * (VulkanDeviceTable): a Direct3D 11 slice store's memory, which the GPU's
 * memory source allocated and the guest writes through the host window
 * (docs/host-memory.md), imported on DXVK's device for it to draw from in
 * place. */
#ifndef AV_VULKAN_BUFFER_HPP
#define AV_VULKAN_BUFFER_HPP

#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;
class VulkanDeviceTable;

class VulkanBuffer {
public:
    VulkanBuffer();
    ~VulkanBuffer();
    VulkanBuffer(const VulkanBuffer &) = delete;
    VulkanBuffer &operator=(const VulkanBuffer &) = delete;

    /* The dma-buf `descriptor` - memory of the GPU's memory source - as a
     * buffer of DXVK's to be drawn from in place
     * (D3d11HostDevice::bind_storage): every use a buffer has, and the device
     * address DXVK takes of every buffer. The descriptor is the memory's once
     * it is made, and stays held otherwise. */
    [[nodiscard]] bool import_dma_buf_to_draw(const VulkanDeviceTable &table, OwnedFd &descriptor, VkDeviceSize size);
    /* The dma-buf `descriptor` - the guest's pages lent to the GPU - as a
     * buffer the GPU copies images into (FrameBufferCopy). The memory is a
     * type the CPU sees the GPU's writes in without an invalidate
     * (VulkanDeviceTable::coherent_memory_type), since the guest maps the same
     * pages cached; false when there is none. As above, the descriptor is the
     * memory's once it is made, and stays held otherwise. */
    [[nodiscard]] bool import_dma_buf_to_receive(const VulkanDeviceTable &table, OwnedFd &descriptor,
                                                 VkDeviceSize size);
    void               release();
    bool               made() const;
    VkBuffer           buffer() const;

private:
    enum class Placement { fastest, coherent };

    [[nodiscard]] bool import(const VulkanDeviceTable &table, OwnedFd &descriptor, VkDeviceSize size,
                              VkBufferUsageFlags usage, Placement placement);

    const VulkanDeviceTable *table_;
    VkBuffer                 buffer_;
    VkDeviceMemory           memory_;
};

}  // namespace av

#endif  // AV_VULKAN_BUFFER_HPP
