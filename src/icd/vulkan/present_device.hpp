/* The host objects behind one swapchain (feature 046, research.md §1): its
 * images, a read-back buffer in host-visible memory the process maps, and the
 * copy of a presented image into it. */
#ifndef AV_PRESENT_DEVICE_HPP
#define AV_PRESENT_DEVICE_HPP

#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class EntryLookup;
struct PresentDeviceProbe;

/* One swapchain image and the memory bound to it. A type of its own so the
 * device's array is a complete type (tools/header_scan.py). */
struct PresentImage {
    VkImage        image_;
    VkDeviceMemory memory_;
};

class PresentDevice {
public:
    static constexpr uint32_t max_images = 8u;
    /* A copy that has not finished in this long has lost its device. */
    static constexpr uint64_t copy_timeout_ns = 5000000000ull;
    /* More semaphores than a present waits on in practice. */
    static constexpr uint32_t max_waits = 8u;

    PresentDevice(EntryLookup &entries, VkDevice device, VkPhysicalDevice physical);
    ~PresentDevice();
    PresentDevice(const PresentDevice &) = delete;
    PresentDevice &operator=(const PresentDevice &) = delete;

    /* A device-local image usable as `usage` and as a copy source, with its
     * memory bound; the device's own error otherwise, with nothing left made. */
    VkResult make_image(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImage &image);
    /* The read-back buffer of `bytes` in host-visible memory, mapped, and the
     * command pool, buffer and fence the copy runs on (queue family 0). */
    VkResult make_readback(VkDeviceSize bytes);
    /* The mapped read-back buffer; null before make_readback. */
    const uint8_t *readback() const;
    /* `image`, in PRESENT_SRC layout, copied into the read-back buffer on
     * `queue` once the `waits` semaphores are signalled, and waited for; the
     * image is left in PRESENT_SRC. */
    VkResult copy(VkQueue queue, VkImage image, VkExtent2D extent, uint32_t wait_count, const VkSemaphore *waits);
    /* Everything made, in reverse. */
    void     release();
    uint32_t images() const;
    /* The image at `index` and the memory bound to it; nothing past the count. */
    PresentImage made(uint32_t index) const;

    bool invariants_hold() const;

private:
    template <typename Function>
    Function call(const char *name);
    /* The first type `mask` allows that has every property in `wanted`. */
    bool     memory_type(uint32_t mask, VkMemoryPropertyFlags wanted, uint32_t &type);
    VkResult bind_image_memory(VkImage image, PresentImage &made);
    VkResult record_copy(VkImage image, VkExtent2D extent);
    void     transition(VkImage image, VkImageLayout from, VkImageLayout to);

    EntryLookup      &entries_;
    VkDevice          device_;
    VkPhysicalDevice  physical_;
    PresentImage      made_[max_images];
    uint32_t          image_count_;
    VkBuffer          buffer_;
    VkDeviceMemory    buffer_memory_;
    uint8_t          *mapped_;
    VkCommandPool     pool_;
    VkCommandBuffer   commands_;
    VkFence           fence_;

    friend struct PresentDeviceProbe;
};

}  // namespace av::icd

#endif  // AV_PRESENT_DEVICE_HPP
