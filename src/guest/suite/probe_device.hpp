/* The probe's Vulkan device (feature 045, T037): an instance, the physical
 * device whose LUID is the adapter's, and a device with one queue of the first
 * family that both draws and computes, made through VulkanEntry and destroyed,
 * device first, when this goes. */
#ifndef AV_PROBE_DEVICE_HPP
#define AV_PROBE_DEVICE_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class VulkanEntry;
struct ProbeDeviceProbe;

class ProbeDevice {
public:
    /* More than a guest has adapters; a longer list is looked at this far. */
    static constexpr uint32_t max_physical_devices = 16u;
    static constexpr uint32_t max_queue_families = 16u;

    explicit ProbeDevice(VulkanEntry &vulkan);
    ~ProbeDevice();
    ProbeDevice(const ProbeDevice &) = delete;
    ProbeDevice &operator=(const ProbeDevice &) = delete;

    /* The window-system extensions an instance and a device are made with:
     * what the present steps call is reachable only when they are enabled. */
    static const char *const  instance_extensions[];
    static const char *const  device_extensions[];
    static constexpr uint32_t instance_extension_count = 2u;
    static constexpr uint32_t device_extension_count = 1u;

    /* At API version 1.3, with the window-system extensions enabled. */
    VkResult make_instance();
    /* The physical devices listed, as far as there is room: VK_INCOMPLETE is
     * a list, not a failure. */
    VkResult list_physical_devices();
    /* Chooses the listed device whose valid LUID is `luid`: whether one is. */
    bool     choose(uint64_t luid);
    /* Chooses the chosen device's first family that draws and computes:
     * whether one does. */
    bool     choose_family();
    /* A device on the chosen one with one queue of the chosen family. */
    VkResult make_device();
    /* The lowest type in `allowed` with every flag in `wanted`: whether one is. */
    bool     memory_type(uint32_t allowed, VkMemoryPropertyFlags wanted, uint32_t &type) const;

    VulkanEntry     &vulkan() const;
    VkInstance       instance() const;
    VkPhysicalDevice physical_device() const;
    VkDevice         device() const;
    VkQueue          queue() const;
    uint32_t         family() const;
    uint32_t         listed() const;

    /* A device only on a chosen physical device, which only an instance lists;
     * a queue only with a device; a family among the types' bounds. */
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

}  // namespace av

#endif  // AV_PROBE_DEVICE_HPP
