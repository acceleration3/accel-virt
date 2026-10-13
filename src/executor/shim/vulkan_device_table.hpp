/* One Vulkan device's entry points and memory types, whoever made the device:
 * the compositor's own (CompositorDevice), or one DXVK made and lends through
 * its interop - Direct3D 11's (DxvkVulkanDevice) or a Direct3D 9 device's
 * (D3d9VulkanLane). Every image, timeline and copy the executor adds to a
 * device goes through the table its device was loaded into
 * (VulkanImage, VulkanTimeline, QueueWork). */
#ifndef AV_VULKAN_DEVICE_TABLE_HPP
#define AV_VULKAN_DEVICE_TABLE_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class VulkanDeviceTable {
public:
    VulkanDeviceTable();
    VulkanDeviceTable(const VulkanDeviceTable &) = delete;
    VulkanDeviceTable &operator=(const VulkanDeviceTable &) = delete;

    /* `device`, on `physical` of `instance`, with the external-memory and
     * timeline entry points this needs; false where it lacks them. The device
     * is not this table's to destroy. */
    [[nodiscard]] bool load(VkInstance instance, VkPhysicalDevice physical, VkDevice device);
    void               forget();
    bool               loaded() const;
    VkDevice           device() const;

    /* Whether the GPU makes an image as `info` describes - its format, type,
     * tiling, usage and flags, its view formats, and its extent - over memory
     * it can export as a dma-buf. */
    [[nodiscard]] bool supports_exportable(const VkImageCreateInfo &info) const;
    /* The first memory type `allowed` permits, a device-local one first;
     * VK_MAX_MEMORY_TYPES for none. */
    uint32_t           memory_type(uint32_t allowed) const;

    [[nodiscard]] bool   create_image(const VkImageCreateInfo &info, VkImage &image) const;
    void                 destroy_image(VkImage image) const;
    VkMemoryRequirements requirements(VkImage image) const;
    /* Memory for `image` alone, `size` bytes of `type`, made or imported as
     * `chain` says, and bound to it. An imported fd is the memory's once the
     * allocation succeeds, even should the bind then fail. */
    [[nodiscard]] bool   bind_dedicated(VkImage image, const void *chain, VkDeviceSize size, uint32_t type,
                                        VkDeviceMemory &memory) const;
    void                 free_memory(VkDeviceMemory memory) const;
    [[nodiscard]] bool   memory_fd(VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits type, int &fd) const;
    /* The first row of a linear image's only subresource. */
    VkSubresourceLayout  row_layout(VkImage image) const;

    [[nodiscard]] bool create_buffer(const VkBufferCreateInfo &info, VkBuffer &buffer) const;
    void               destroy_buffer(VkBuffer buffer) const;
    VkMemoryRequirements buffer_requirements(VkBuffer buffer) const;
    /* The memory types a dma-buf `fd` may be imported as; 0 for none. */
    uint32_t           dma_buf_types(int fd) const;
    /* The memory types `pointer`, host memory of this process, may be
     * imported as; 0 for none, or a device without VK_EXT_external_memory_host. */
    uint32_t           host_pointer_types(void *pointer) const;
    /* Memory for `buffer` alone, as bind_dedicated makes it for an image. */
    [[nodiscard]] bool bind_buffer(VkBuffer buffer, const void *chain, VkDeviceSize size, uint32_t type,
                                   VkDeviceMemory &memory) const;
    /* The same, memory not dedicated to the buffer: an import of a host
     * pointer may not be. */
    [[nodiscard]] bool bind_buffer_shared(VkBuffer buffer, const void *chain, VkDeviceSize size, uint32_t type,
                                          VkDeviceMemory &memory) const;
    [[nodiscard]] bool create_semaphore(const void *chain, VkSemaphore &semaphore) const;
    void               destroy_semaphore(VkSemaphore semaphore) const;
    [[nodiscard]] bool import_semaphore_fd(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits type,
                                           int fd) const;
    [[nodiscard]] bool semaphore_fd(VkSemaphore semaphore, VkExternalSemaphoreHandleTypeFlagBits type,
                                    int &fd) const;
    /* Waits for `semaphore` to reach `value`, at most `nanoseconds`. */
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
    void               copy_buffer(VkCommandBuffer buffer, VkBuffer source, VkBuffer destination, uint32_t count,
                                   const VkBufferCopy *regions) const;
    /* Every write recorded before this, readable by transfers after it. */
    void               to_transfer(VkCommandBuffer buffer) const;
    /* Every transfer write recorded before this, available to the host. */
    void               to_host(VkCommandBuffer buffer) const;
    void               copy_to_buffer(VkCommandBuffer buffer, VkImage source, VkBuffer destination, uint32_t count,
                                      const VkBufferImageCopy *regions) const;
    [[nodiscard]] bool submit(VkQueue queue, const VkSubmitInfo &submit) const;
    void               queue_idle(VkQueue queue) const;

private:
    VkPhysicalDevice                 physical_;
    VkDevice                         device_;
    VkPhysicalDeviceMemoryProperties memory_;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 format_properties_;
    PFN_vkCreateImage                create_image_;
    PFN_vkDestroyImage               destroy_image_;
    PFN_vkGetImageMemoryRequirements requirements_;
    PFN_vkAllocateMemory             allocate_;
    PFN_vkFreeMemory                 free_;
    PFN_vkBindImageMemory            bind_;
    PFN_vkGetImageSubresourceLayout  layout_;
    PFN_vkGetMemoryFdKHR             memory_fd_;
    PFN_vkCreateBuffer               create_buffer_;
    PFN_vkDestroyBuffer              destroy_buffer_;
    PFN_vkGetBufferMemoryRequirements buffer_requirements_;
    PFN_vkBindBufferMemory           bind_buffer_;
    PFN_vkGetMemoryFdPropertiesKHR   fd_properties_;
    PFN_vkGetMemoryHostPointerPropertiesEXT host_properties_;
    PFN_vkCmdCopyImageToBuffer       copy_to_buffer_;
    PFN_vkCmdCopyBuffer              copy_buffer_;
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
};

}  // namespace av

#endif  // AV_VULKAN_DEVICE_TABLE_HPP
