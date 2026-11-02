#ifndef AV_VULKAN_DEVICE_TABLE_HPP
#define AV_VULKAN_DEVICE_TABLE_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class OwnedFd;

class VulkanDeviceTable {
public:
    VulkanDeviceTable();
    VulkanDeviceTable(const VulkanDeviceTable &) = delete;
    VulkanDeviceTable &operator=(const VulkanDeviceTable &) = delete;

    [[nodiscard]] bool load(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    void               forget();
    bool               loaded() const;
    bool               interoperates() const;
    bool               imports_dma_bufs() const;
    VkDevice           device() const;
    [[nodiscard]] bool lose() const;

    [[nodiscard]] bool supports_exportable(const VkImageCreateInfo &info) const;
    uint32_t           tiled_modifiers(const VkImageCreateInfo &rows, uint64_t *modifiers, uint32_t capacity) const;
    uint64_t           modifier_of(VkImage image) const;
    uint32_t           memory_type(uint32_t allowed) const;
    uint32_t           raw_memory_type(uint32_t allowed) const;
    uint32_t           system_memory_type(uint32_t allowed) const;
    void                  memory_properties(VkPhysicalDeviceMemoryProperties &answer) const;
    [[nodiscard]] bool    allocate_exportable(VkDeviceSize size, uint32_t type, VkDeviceMemory &memory) const;
    [[nodiscard]] bool    allocate_exportable(VkImage image, VkDeviceSize size, uint32_t type,
                                              VkDeviceMemory &memory) const;
    [[nodiscard]] bool    import_memory(OwnedFd &descriptor, VkExternalMemoryHandleTypeFlagBits handle,
                                        VkDeviceSize size, uint32_t type, const void *next,
                                        VkDeviceMemory &memory) const;
    [[nodiscard]] bool    map_memory(VkDeviceMemory memory, void *&pointer) const;

    [[nodiscard]] bool   create_image(const VkImageCreateInfo &info, VkImage &image) const;
    void                 destroy_image(VkImage image) const;
    VkMemoryRequirements requirements(VkImage image) const;
    [[nodiscard]] bool   bind_image(VkImage image, VkDeviceMemory memory) const;
    void                 free_memory(VkDeviceMemory memory) const;
    [[nodiscard]] bool   memory_fd(VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits type, int &fd) const;
    VkSubresourceLayout  row_layout(VkImage image) const;

    [[nodiscard]] bool create_buffer(const VkBufferCreateInfo &info, VkBuffer &buffer) const;
    void               destroy_buffer(VkBuffer buffer) const;
    VkMemoryRequirements buffer_requirements(VkBuffer buffer) const;
    uint32_t           dma_buf_types(int fd) const;
    [[nodiscard]] bool bind_buffer(VkBuffer buffer, VkDeviceMemory memory) const;
    [[nodiscard]] bool create_semaphore(const void *chain, VkSemaphore &semaphore) const;
    void               destroy_semaphore(VkSemaphore semaphore) const;
    [[nodiscard]] bool import_semaphore_fd(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits type,
                                           int fd) const;
    [[nodiscard]] bool semaphore_fd(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits type,
                                    int &fd) const;
    [[nodiscard]] bool wait_semaphore(VkSemaphore semaphore, uint64_t value, uint64_t nanoseconds) const;

    [[nodiscard]] bool create_command_pool(uint32_t family, VkCommandPool &pool) const;
    void               destroy_command_pool(VkCommandPool pool) const;
    [[nodiscard]] bool allocate_command_buffers(VkCommandPool pool, uint32_t count, VkCommandBuffer *buffers) const;
    [[nodiscard]] bool begin(VkCommandBuffer buffer) const;
    [[nodiscard]] bool end(VkCommandBuffer buffer) const;
    void               reset(VkCommandBuffer buffer) const;
    void               barrier(VkCommandBuffer buffer, VkPipelineStageFlags from, VkPipelineStageFlags to,
                               uint32_t count, const VkImageMemoryBarrier *barriers) const;
    void               copy_image(VkCommandBuffer buffer, VkImage source, VkImage destination,
                                  const VkImageCopy &region) const;
    [[nodiscard]] bool submit(VkQueue queue, const VkSubmitInfo &submit) const;
    void               queue_idle(VkQueue queue) const;

private:
    bool supports_layout(const VkImageCreateInfo &info, const VkImageFormatListCreateInfo *views,
                         const uint64_t *modifier) const;

    VkPhysicalDevice                 physical_;
    VkDevice                         device_;
    VkPhysicalDeviceMemoryProperties memory_;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 format_properties_;
    PFN_vkGetPhysicalDeviceFormatProperties2      format_features_;
    PFN_vkGetImageDrmFormatModifierPropertiesEXT  image_modifier_;
    PFN_vkCreateImage                create_image_;
    PFN_vkDestroyImage               destroy_image_;
    PFN_vkGetImageMemoryRequirements requirements_;
    PFN_vkAllocateMemory             allocate_;
    PFN_vkFreeMemory                 free_;
    PFN_vkMapMemory                  map_;
    PFN_vkBindImageMemory            bind_;
    PFN_vkGetImageSubresourceLayout  layout_;
    PFN_vkGetMemoryFdKHR             memory_fd_;
    PFN_vkCreateBuffer               create_buffer_;
    PFN_vkDestroyBuffer              destroy_buffer_;
    PFN_vkGetBufferMemoryRequirements buffer_requirements_;
    PFN_vkBindBufferMemory           bind_buffer_;
    PFN_vkGetMemoryFdPropertiesKHR   fd_properties_;
    PFN_vkCreateSemaphore            create_semaphore_;
    PFN_vkDestroySemaphore           destroy_semaphore_;
    PFN_vkImportSemaphoreFdKHR       import_semaphore_;
    PFN_vkGetSemaphoreFdKHR          semaphore_fd_;
    PFN_vkWaitSemaphores             wait_semaphores_;
    PFN_vkCreateCommandPool          create_pool_;
    PFN_vkDestroyCommandPool         destroy_pool_;
    PFN_vkAllocateCommandBuffers     allocate_buffers_;
    PFN_vkBeginCommandBuffer         begin_;
    PFN_vkEndCommandBuffer           end_;
    PFN_vkResetCommandBuffer         reset_;
    PFN_vkCmdPipelineBarrier         barrier_;
    PFN_vkCmdCopyImage               copy_image_;
    PFN_vkQueueSubmit                submit_;
    PFN_vkQueueWaitIdle              queue_idle_;
    void (*lose_)(VkDevice device);
};

}

#endif
