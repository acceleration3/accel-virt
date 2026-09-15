/* The host Vulkan device a scenario works on (feature 044, US1): an instance,
 * the first of its physical devices and a device with one queue of family 0,
 * each encoded by the generated client and its step reported - instance,
 * physical_device with how many there are, device - and destroyed in reverse by
 * destroy(). The handles are the host's, passed back as the guest got them. */
#ifndef AV_VULKAN_DEVICE_CHAIN_HPP
#define AV_VULKAN_DEVICE_CHAIN_HPP

#include "core/support/status.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class ClientTransport;
class ScenarioSteps;
struct VulkanDeviceChainProbe;

class VulkanDeviceChain {
public:
    VulkanDeviceChain(ScenarioSteps &steps, ClientTransport &transport);
    VulkanDeviceChain(const VulkanDeviceChain &) = delete;
    VulkanDeviceChain &operator=(const VulkanDeviceChain &) = delete;

    /* The first step that failed, with nothing after it attempted. */
    [[nodiscard]] Status make();
    /* Sends the destroys of whatever was made - asynchronously, so a caller
     * proves they ran with a round trip after them - and forgets it. */
    void                 destroy();
    VkPhysicalDevice     physical_device() const;
    /* Null until made. */
    VkDevice             device() const;

    bool invariants_hold() const;

private:
    /* Twice the host GPUs the executor keeps (HostGpuSet::max_gpus); the
     * first device is the one used. */
    static constexpr uint32_t max_physical_devices = 16u;

    [[nodiscard]] Status make_instance();
    [[nodiscard]] Status find_physical_device();
    [[nodiscard]] Status make_device();

    ScenarioSteps   &steps_;
    ClientTransport &transport_;
    uint64_t         instance_;
    uint64_t         physical_device_;
    uint64_t         device_;

    friend struct VulkanDeviceChainProbe;
};

}  // namespace av

#endif  // AV_VULKAN_DEVICE_CHAIN_HPP
