#ifndef AV_PROBE_DEVICE_HPP
#define AV_PROBE_DEVICE_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class VulkanEntry;
struct ProbeDeviceProbe;

class ProbeDevice {
public:
    static constexpr uint32_t max_physical_devices = 16u;
    static constexpr uint32_t max_queue_families = 16u;

    explicit ProbeDevice(VulkanEntry &vulkan);
    ~ProbeDevice();
    ProbeDevice(const ProbeDevice &) = delete;
    ProbeDevice &operator=(const ProbeDevice &) = delete;

    static const char *const  instance_extensions[];
    static const char *const  device_extensions[];
    static constexpr uint32_t instance_extension_count = 2u;
    static constexpr uint32_t device_extension_count = 1u;
    static constexpr uint32_t max_device_extensions = 8u;

    VkResult make_instance();
    VkResult list_physical_devices();
    bool     choose(uint64_t luid);
    bool     choose_family();
    VkResult make_device(const char *const *extra = nullptr, uint32_t extra_count = 0u);
    bool     memory_type(uint32_t allowed, VkMemoryPropertyFlags wanted, uint32_t &type) const;

    VulkanEntry     &vulkan() const;
    VkInstance       instance() const;
    VkPhysicalDevice physical_device() const;
    VkDevice         device() const;
    VkQueue          queue() const;
    uint32_t         family() const;
    uint32_t         listed() const;

    bool invariants_hold() const;

private:
    VulkanEntry                     &vulkan_;
    VkInstance                       instance_;
    VkPhysicalDevice                 listed_[max_physical_devices];
    uint32_t                         listed_count_;
    VkPhysicalDevice                 physical_device_;
    VkDevice                         device_;
    VkQueue                          queue_;
    uint32_t                         family_;
    VkPhysicalDeviceMemoryProperties memory_;

    friend struct ProbeDeviceProbe;
};

}

#endif
