/* The Vulkan functions a swapchain calls on the application's device, as a
 * test scripts them (feature 046): images and a read-back buffer of bytes the
 * test paints and reads, the commands a copy records, and a refusal at any
 * call. The entry points are bare functions, as a driver's are, and each is one
 * line into the bound device - the shape of the ICD's own binding. */
#ifndef AV_SCRIPTED_VULKAN_DEVICE_HPP
#define AV_SCRIPTED_VULKAN_DEVICE_HPP

#include "icd/vulkan/entry_lookup.hpp"

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace av {

class ScriptedVulkanDevice final : public icd::EntryLookup {
public:
    /* Room for every swapchain a process may hold, of two images each. */
    static constexpr std::uint32_t max_images = 32u;
    static constexpr std::uint32_t max_memories = 64u;
    /* The largest frame a test paints: 64x64 in four bytes a pixel. */
    static constexpr std::uint32_t max_image_bytes = 64u * 64u * 4u;
    static constexpr std::uint32_t max_barriers = 16u;

    struct Barrier {
        VkImage       image;
        VkImageLayout from;
        VkImageLayout to;
    };

    /* The newest device is the bound one: the entry points have nowhere to put
     * a `this`, as a loader's do not, and its destruction binds the one it was
     * constructed under again. */
    ScriptedVulkanDevice();
    ~ScriptedVulkanDevice();
    ScriptedVulkanDevice(const ScriptedVulkanDevice &) = delete;
    ScriptedVulkanDevice &operator=(const ScriptedVulkanDevice &) = delete;

    PFN_vkVoidFunction entry(const char *name) override;

    VkDevice         device();
    VkPhysicalDevice physical();
    VkQueue          queue();

    /* The `nth` call of `name` answers `code`; nth counts from 1. */
    void refuse(const char *name, std::uint32_t nth, VkResult code);
    /* No memory type has the device-local property, or none has host-visible,
     * or the requirements allow no type at all. */
    void without_device_local();
    void without_host_visible();
    void allow_no_memory_type();
    /* A host that names more memory types than Vulkan has. */
    void overcount_memory_types();
    /* A copy that never finishes. */
    void stall_copy();

    /* Every byte of `image` set to one RGBA pixel repeated. */
    void paint(VkImage image, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
    /* Row `row` alone, of an image `row_bytes` a row, one colour. */
    void paint_row(VkImage image, std::uint32_t row, std::uint32_t row_bytes, std::uint8_t red, std::uint8_t green,
                   std::uint8_t blue, std::uint8_t alpha);

    std::uint32_t       images() const;
    std::uint32_t       memories() const;
    std::uint32_t       submits() const;
    std::uint32_t       last_wait_count() const;
    VkSemaphore         last_wait() const;
    std::uint32_t       barriers() const;
    const Barrier      &barrier(std::uint32_t index) const;
    std::uint32_t       copies() const;
    /* The read-back buffer's bytes, as the copy left them. */
    const std::uint8_t *readback() const;
    std::uint32_t       mappings() const;
    std::uint32_t       pools() const;
    /* The queue family the command pool was made on. */
    std::uint32_t       pool_family() const;
    std::uint32_t       fences() const;

    /* What a bound entry point calls; public because the entry points are
     * free functions of the harness's translation unit. */
    VkResult make_image(const VkImageCreateInfo *info, VkImage *image);
    void     destroy_image(VkImage image);
    void     image_requirements(VkImage image, VkMemoryRequirements *needs);
    void     buffer_requirements(VkBuffer buffer, VkMemoryRequirements *needs);
    void     memory_properties(VkPhysicalDeviceMemoryProperties *properties);
    VkResult allocate(const VkMemoryAllocateInfo *info, VkDeviceMemory *memory);
    void     free_memory(VkDeviceMemory memory);
    VkResult bind_image(VkImage image, VkDeviceMemory memory);
    VkResult bind_buffer(VkBuffer buffer, VkDeviceMemory memory);
    VkResult make_buffer(const VkBufferCreateInfo *info, VkBuffer *buffer);
    void     destroy_buffer(VkBuffer buffer);
    VkResult map(VkDeviceMemory memory, void **view);
    void     unmap(VkDeviceMemory memory);
    VkResult make_pool(const VkCommandPoolCreateInfo *info, VkCommandPool *pool);
    void     destroy_pool(VkCommandPool pool);
    VkResult make_commands(const VkCommandBufferAllocateInfo *info, VkCommandBuffer *commands);
    VkResult make_fence(VkFence *fence);
    void     destroy_fence(VkFence fence);
    VkResult reset_fences();
    VkResult wait_fences(std::uint64_t timeout);
    VkResult begin(VkCommandBuffer commands);
    VkResult end(VkCommandBuffer commands);
    void     record_barrier(std::uint32_t count, const VkImageMemoryBarrier *barriers);
    void     record_copy(VkImage image, const VkBufferImageCopy *region);
    VkResult submit(VkQueue queue, const VkSubmitInfo *info, VkFence fence);

private:
    struct Image {
        bool          made;
        std::uint32_t width;
        std::uint32_t height;
        VkFormat      format;
        VkImageUsageFlags usage;
        std::uint8_t  bytes[max_image_bytes];
    };
    struct Memory {
        bool          made;
        VkDeviceSize  size;
        std::uint32_t type;
    };
    struct Copy {
        VkImage    image;
        VkExtent3D extent;
    };
    struct Refusal {
        const char   *name;
        std::uint32_t nth;
        VkResult      code;
    };

    /* VK_SUCCESS, or what a test scripted for this call. */
    VkResult      judge(const char *name);
    Image        *image_of(VkImage image);

    ScriptedVulkanDevice *outer_;
    Refusal       refusal_;
    std::uint32_t calls_;
    Image         images_[max_images];
    Memory        memories_[max_memories];
    std::uint8_t  buffer_bytes_[max_image_bytes];
    VkDeviceSize  buffer_size_;
    bool          buffer_made_;
    bool          device_local_;
    bool          host_visible_;
    bool          any_type_;
    bool          overcount_;
    bool          stall_;
    bool          pool_made_;
    bool          fence_made_;
    bool          recording_;
    std::uint32_t image_count_;
    std::uint32_t memory_count_;
    std::uint32_t mappings_;
    std::uint32_t pools_;
    std::uint32_t fences_;
    std::uint32_t submits_;
    std::uint32_t wait_count_;
    VkSemaphore   wait_;
    std::uint32_t family_;
    Barrier       barriers_[max_barriers];
    std::uint32_t barrier_count_;
    Copy          copies_[max_barriers];
    std::uint32_t copy_count_;
};

}  // namespace av

#endif  // AV_SCRIPTED_VULKAN_DEVICE_HPP
