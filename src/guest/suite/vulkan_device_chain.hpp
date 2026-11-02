#ifndef AV_VULKAN_DEVICE_CHAIN_HPP
#define AV_VULKAN_DEVICE_CHAIN_HPP

#include "core/support/status.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"

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

    [[nodiscard]] Status make();
    void                 destroy();
    VkPhysicalDevice     physical_device() const;
    VkDevice             device() const;
    vkproto::HandleTranslation &handles();

    bool invariants_hold() const;

private:
    static constexpr uint32_t max_physical_devices = 16u;

    [[nodiscard]] Status make_instance();
    [[nodiscard]] Status find_physical_device();
    [[nodiscard]] Status make_device();

    ScenarioSteps   &steps_;
    ClientTransport &transport_;
    vkproto::HandleIdentity handles_;
    uint64_t         instance_;
    uint64_t         physical_device_;
    uint64_t         device_;

    friend struct VulkanDeviceChainProbe;
};

}

#endif
