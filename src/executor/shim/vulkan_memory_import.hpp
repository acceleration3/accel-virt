#ifndef AV_VULKAN_MEMORY_IMPORT_HPP
#define AV_VULKAN_MEMORY_IMPORT_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;
class VulkanDeviceTable;

class VulkanMemoryImport {
public:
    explicit VulkanMemoryImport(const VulkanDeviceTable &table);
    VulkanMemoryImport(const VulkanMemoryImport &) = delete;
    VulkanMemoryImport &operator=(const VulkanMemoryImport &) = delete;

    bool               imports() const;
    void               release(uint64_t memory);
    uint32_t           dmabuf_types(int fd) const;
    [[nodiscard]] bool import_dmabuf(OwnedFd &descriptor, uint64_t size, uint32_t memory_type, uint64_t &memory);
    VkExternalMemoryHandleTypeFlags buffer_external_types(VkPhysicalDevice physical, const VkBufferCreateInfo &create,
                                                          VkExternalMemoryHandleTypeFlagBits type);
    VkExternalMemoryHandleTypeFlags image_external_types(VkPhysicalDevice physical, const VkImageCreateInfo &create,
                                                         VkExternalMemoryHandleTypeFlagBits type);

    [[nodiscard]] bool export_memory(uint64_t memory, VkExternalMemoryHandleTypeFlagBits type, OwnedFd &descriptor);
    [[nodiscard]] bool export_semaphore(uint64_t semaphore, OwnedFd &descriptor);
    [[nodiscard]] bool import_shared_memory(int fd, VkExternalMemoryHandleTypeFlagBits type, uint64_t size,
                                            uint32_t memory_type, uint64_t &memory);
    [[nodiscard]] bool import_shared_semaphore(int fd, uint64_t &semaphore);
    void               destroy_semaphore(uint64_t semaphore);
    [[nodiscard]] bool import_semaphore_payload(int fd, uint64_t semaphore);

    uint32_t held() const;

private:
    const VulkanDeviceTable &table_;
    uint32_t                 held_;
};

}

#endif
