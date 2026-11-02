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

    [[nodiscard]] bool import_dma_buf_to_draw(const VulkanDeviceTable &table, OwnedFd &descriptor, VkDeviceSize size);
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

}

#endif
