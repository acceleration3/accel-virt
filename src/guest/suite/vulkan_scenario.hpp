/* The Vulkan object chain on the host GPU from a guest process (feature 044,
 * US1): an instance, its physical devices, a device with one queue, a buffer,
 * memory for it and the binding, each encoded by the generated client and run
 * by the host's executor over one Vulkan context's transport. Every object
 * made is destroyed in reverse whatever failed, and a transport no-op after the
 * destroys - which the host sends no answer for - proves the context ran them
 * all without faulting. */
#ifndef AV_VULKAN_SCENARIO_HPP
#define AV_VULKAN_SCENARIO_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"

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
    /* Twice the host GPUs the executor keeps (HostGpuSet::max_gpus); the
     * first device is the one used. */
    static constexpr uint32_t max_physical_devices = 16u;

    [[nodiscard]] Status make_device();
    [[nodiscard]] Status find_physical_device();
    [[nodiscard]] Status bind_buffer();
    [[nodiscard]] Status allocate_memory();
    /* The no-op's status after every destroy was sent. */
    [[nodiscard]] Status destroy();

    ScenarioSteps     steps_;
    ScenarioTransport link_;
    uint64_t          instance_;
    uint64_t          physical_device_;
    uint64_t          device_;
    VkBuffer          buffer_;
    VkDeviceMemory    memory_;
};

}  // namespace av

#endif  // AV_VULKAN_SCENARIO_HPP
