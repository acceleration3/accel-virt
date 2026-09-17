/* rvkvkprobe's proof (feature 045, T037), over whatever Vulkan it is handed:
 * in the guest the Khronos loader, which finds rvkvk.dll through the
 * adapter's registration; on the full-stack rig the ICD's own entry point.
 * Steps, as tools/guest/run_guest_suite.py reads them: instance; adapter_luid
 * and physical_device, each the LowPart of a LUID in decimal - the rvkgpu
 * adapter's, from D3DKMT, and the Vulkan device's that carries it; device;
 * compute and draw, each how many values came back right; lost, a forced
 * context fault answered as VK_ERROR_DEVICE_LOST by the fault and by the next
 * call; recreated, the compute again on a new instance. A step that cannot
 * run past a failure ends the run there. */
#ifndef AV_VULKAN_PROBE_HPP
#define AV_VULKAN_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/suite/compute_work.hpp"
#include "guest/suite/draw_work.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class ImageSink;

class GuestReport;
class Kmt;
class ProbeDevice;
class VulkanEntry;

class VulkanProbe {
public:
    static constexpr uint32_t elements = ComputeWork::elements;
    static constexpr uint32_t extent = DrawWork::extent;

    /* `images` keeps the draw step's picture. */
    VulkanProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ImageSink &images);
    VulkanProbe(const VulkanProbe &) = delete;
    VulkanProbe &operator=(const VulkanProbe &) = delete;

    void run();

private:
    /* The rvkgpu adapter's LUID into `adapter_luid_`. */
    [[nodiscard]] Status adapter_step();
    [[nodiscard]] Status physical_device_step(ProbeDevice &device);
    [[nodiscard]] Status device_step(ProbeDevice &device);
    void                 lost_step(ProbeDevice &device);
    void                 recreated_step();

    Kmt         &kmt_;
    VulkanEntry &vulkan_;
    ImageSink   &images_;
    ScenarioSteps steps_;
    uint64_t     adapter_luid_;
};

}  // namespace av

#endif  // AV_VULKAN_PROBE_HPP
