#ifndef AV_VULKAN_SCENARIO_HPP
#define AV_VULKAN_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "guest/suite/vulkan_device_chain.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class VulkanScenario {
public:
    static constexpr VkDeviceSize buffer_bytes = 4096u;

    VulkanScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    VulkanScenario(const VulkanScenario &) = delete;
    VulkanScenario &operator=(const VulkanScenario &) = delete;

    void run();

private:
    [[nodiscard]] Status bind_buffer();
    [[nodiscard]] Status allocate_memory();
    [[nodiscard]] Status destroy();

    ScenarioSteps     steps_;
    ScenarioTransport link_;
    VulkanDeviceChain chain_;
    VkBuffer          buffer_;
    VkDeviceMemory    memory_;
};

}

#endif
