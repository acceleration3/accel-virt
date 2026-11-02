#ifndef AV_VULKAN_IMAGE_HPP
#define AV_VULKAN_IMAGE_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;
class VulkanDeviceTable;

class VulkanImage {
public:
    VulkanImage();
    ~VulkanImage();
    VulkanImage(const VulkanImage &) = delete;
    VulkanImage &operator=(const VulkanImage &) = delete;

    [[nodiscard]] bool   create(const VulkanDeviceTable &table, const VkImageCreateInfo &info);
    VkMemoryRequirements requirements() const;
    [[nodiscard]] bool   bind_exportable(uint32_t type);
    [[nodiscard]] bool   bind_imported(OwnedFd &descriptor, VkExternalMemoryHandleTypeFlagBits handle,
                                       VkDeviceSize size, uint32_t type);

    [[nodiscard]] bool make_exportable(const VulkanDeviceTable &table, const VkImageCreateInfo &info);
    [[nodiscard]] bool import_dma_buf(const VulkanDeviceTable &table, const VkImageCreateInfo &info,
                                      OwnedFd &descriptor);
    [[nodiscard]] bool export_dma_buf(OwnedFd &descriptor) const;
    uint64_t            modifier() const;
    VkSubresourceLayout row_layout() const;
    void                release();

    bool           made() const;
    VkImage        image() const;
    VkDeviceMemory memory() const;
    VkDeviceSize   size() const;

private:
    [[nodiscard]] bool bound(VkDeviceSize size);

    const VulkanDeviceTable *table_;
    VkImage                  image_;
    VkDeviceMemory           memory_;
    VkDeviceSize             size_;
};

}

#endif
