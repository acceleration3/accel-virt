#ifndef AV_SHARED_OBJECT_PROBE_HPP
#define AV_SHARED_OBJECT_PROBE_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ClEntry;
class GuestReport;
class Kmt;
class ProbeDevice;
class VulkanEntry;

class SharedObjectProbe {
public:
    static constexpr uint64_t bytes = 4u * 4096u;
    static constexpr uint32_t max_extensions = 512u;
    static constexpr uint32_t cl_extension_bytes = 4096u;

    SharedObjectProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ClEntry *cl);
    SharedObjectProbe(const SharedObjectProbe &) = delete;
    SharedObjectProbe &operator=(const SharedObjectProbe &) = delete;

    void run();

private:
    bool reported_step(ProbeDevice &device);
    void carrier_steps();
    void carrier_size_steps(uint32_t device);
    bool target_step(ProbeDevice &device);
    bool exportable_step(ProbeDevice &device, VkDeviceMemory &memory);
    void fill_step(ProbeDevice &device, VkDeviceMemory memory);
    bool handle_step(ProbeDevice &device, VkDeviceMemory memory, void *&handle);
    void released_step(ProbeDevice &device, VkDeviceMemory memory);
    void opencl_steps(void *handle);

    Kmt          &kmt_;
    VulkanEntry  &vulkan_;
    ClEntry      *cl_;
    VkBuffer      target_;
    uint32_t      target_types_;
    bool          filled_;
    ScenarioSteps steps_;
};

}

#endif
