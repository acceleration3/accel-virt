#ifndef AV_PRESENT_DEVICE_HPP
#define AV_PRESENT_DEVICE_HPP

#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class EntryLookup;
struct PresentDeviceProbe;

struct PresentImage {
    VkImage        image_;
    VkDeviceMemory memory_;
};

class PresentDevice {
public:
    static constexpr uint32_t max_images = 8u;
    static constexpr uint32_t slots = 2u;
    static constexpr uint64_t copy_timeout_ns = 5000000000ull;
    static constexpr uint32_t max_waits = 8u;

    PresentDevice(EntryLookup &entries, VkDevice device, VkPhysicalDevice physical);
    ~PresentDevice();
    PresentDevice(const PresentDevice &) = delete;
    PresentDevice &operator=(const PresentDevice &) = delete;

    VkResult make_image(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImage &image);
    VkResult make_readback(VkDeviceSize bytes);
    VkResult make_commands();
    const uint8_t *readback(uint32_t slot) const;
    VkResult copy(VkQueue queue, VkImage image, VkExtent2D extent, uint32_t wait_count, const VkSemaphore *waits,
                  uint32_t slot);
    VkResult copy_into(VkQueue queue, VkImage image, VkImage target, VkExtent2D extent, uint32_t wait_count,
                       const VkSemaphore *waits, uint32_t slot);
    VkResult finish(uint32_t slot);
    VkFence  fence(uint32_t slot) const;
    void     doubt(uint32_t slot);
    VkResult skip(VkQueue queue, uint32_t wait_count, const VkSemaphore *waits);
    void     release();
    uint32_t images() const;
    PresentImage made(uint32_t index) const;

    bool invariants_hold() const;

private:
    template <typename Function>
    Function call(const char *name);
    bool     memory_type(uint32_t mask, VkMemoryPropertyFlags wanted, uint32_t &type);
    VkResult bind_image_memory(VkImage image, PresentImage &made);
    VkResult claim(uint32_t slot);
    VkResult record_copy(VkImage image, VkExtent2D extent, uint32_t slot);
    VkResult record_copy_into(VkImage image, VkImage target, VkExtent2D extent, uint32_t slot);
    VkResult run(VkQueue queue, uint32_t wait_count, const VkSemaphore *waits, uint32_t slot);
    void     transition(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to);
    void     release_to_external(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to);

    EntryLookup      &entries_;
    VkDevice          device_;
    VkPhysicalDevice  physical_;
    PresentImage      made_[max_images];
    uint32_t          image_count_;
    VkBuffer          buffer_;
    VkDeviceMemory    buffer_memory_;
    uint8_t          *mapped_;
    VkDeviceSize      slot_bytes_;
    VkCommandPool     pool_;
    VkCommandBuffer   commands_[slots];
    VkFence           fences_[slots];
    bool              doubted_[slots];

    friend struct PresentDeviceProbe;
};

}

#endif
