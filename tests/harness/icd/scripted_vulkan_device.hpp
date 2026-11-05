#ifndef AV_SCRIPTED_VULKAN_DEVICE_HPP
#define AV_SCRIPTED_VULKAN_DEVICE_HPP

#include "icd/vulkan/entry_lookup.hpp"

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace av {

class ScriptedVulkanDevice final : public icd::EntryLookup {
public:
    static constexpr std::uint32_t max_images = 32u;
    static constexpr std::uint32_t max_memories = 64u;
    static constexpr std::uint32_t max_image_bytes = 64u * 64u * 4u;
    static constexpr std::uint32_t max_barriers = 16u;

    struct Barrier {
        VkImage       image;
        VkImageLayout from;
        VkImageLayout to;
        std::uint32_t to_family;
    };
    struct Described {
        VkFormat                         format;
        VkImageCreateFlags               flags;
        VkImageTiling                    tiling;
        VkImageUsageFlags                usage;
        VkExternalMemoryHandleTypeFlags  external;
        std::uint32_t                    view_formats;
        std::uint64_t                    modifier;
        VkDeviceSize                     row_pitch;
        VkDeviceMemory                   bound;
    };

    ScriptedVulkanDevice();
    ~ScriptedVulkanDevice();
    ScriptedVulkanDevice(const ScriptedVulkanDevice &) = delete;
    ScriptedVulkanDevice &operator=(const ScriptedVulkanDevice &) = delete;

    PFN_vkVoidFunction entry(const char *name) override;

    VkDevice         device();
    VkPhysicalDevice physical();
    VkQueue          queue();

    void refuse(const char *name, std::uint32_t nth, VkResult code);
    void without_device_local();
    void without_host_visible();
    void allow_no_memory_type();
    void overcount_memory_types();
    void stall_copy();

    void paint(VkImage image, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
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
    std::uint32_t       image_copies() const;
    Described           described(VkImage image) const;
    const std::uint8_t *bytes(VkImage image) const;
    const std::uint8_t *readback() const;
    std::uint32_t       mappings() const;
    std::uint32_t       pools() const;
    std::uint32_t       pool_family() const;
    std::uint32_t       fences() const;

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
    void     record_image_copy(VkImage source, VkImage target, const VkImageCopy *region);
    VkResult submit(VkQueue queue, const VkSubmitInfo *info, VkFence fence);

private:
    struct Image {
        bool          made;
        std::uint32_t width;
        std::uint32_t height;
        Described     described;
        std::uint8_t  bytes[max_image_bytes];
    };
    struct Memory {
        bool          made;
        VkDeviceSize  size;
        std::uint32_t type;
    };
    struct Copy {
        VkImage      image;
        VkExtent3D   extent;
        VkDeviceSize offset;
    };
    struct ImageCopy {
        VkImage    source;
        VkImage    target;
        VkExtent3D extent;
    };
    struct Refusal {
        const char   *name;
        std::uint32_t nth;
        VkResult      code;
    };

    VkResult      judge(const char *name);
    Image        *image_of(VkImage image);
    const Image  *image_of(VkImage image) const;

    ScriptedVulkanDevice *outer_;
    Refusal       refusal_;
    std::uint32_t calls_;
    Image         images_[max_images];
    Memory        memories_[max_memories];
    std::uint8_t  buffer_bytes_[max_image_bytes];
    VkDeviceSize  buffer_size_;
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
    ImageCopy     image_copies_[max_barriers];
    std::uint32_t image_copy_count_;
};

}

#endif
