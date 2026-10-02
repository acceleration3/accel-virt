/* avvkprobe's proof (features 045 and 046), over whatever Vulkan it is handed:
 * in the guest the Khronos loader, which finds avvk.dll through the
 * adapter's registration; on the full-stack rig the ICD's own entry point.
 * Steps, as tools/guest/run_guest_suite.py reads them: instance; adapter_luid
 * and physical_device, each the LowPart of a LUID in decimal - the avgpu
 * adapter's, from D3DKMT, and the Vulkan device's that carries it; device;
 * compute and draw, each how many values came back right; lost, a forced
 * context fault answered as VK_ERROR_DEVICE_LOST by the fault and by the next
 * call; recreated, the compute again on a new instance. Then the window-system
 * steps (feature 046), which need the probe's own window: surface, what the
 * window's surface offers; present, the triangle presented sixty times and read
 * back off the window itself; pacing, how many of those frames the driver held
 * to a vertical blank; resized, a window that changed size answered out of date
 * and presented again from a new swapchain. A step that cannot run past a
 * failure ends the run there. */
#ifndef AV_VULKAN_PROBE_HPP
#define AV_VULKAN_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/probe/compute_work.hpp"
#include "guest/probe/draw_work.hpp"
#include "guest/probe/present_work.hpp"
#include "guest/probe/vblank_clock.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class ImageSink;

class GuestReport;
class Kmt;
class ProbeDevice;
class VulkanEntry;
class WindowFrame;

class VulkanProbe {
public:
    static constexpr uint32_t elements = ComputeWork::elements;
    static constexpr uint32_t extent = DrawWork::extent;
    /* The frames the present step draws: enough that a paced run is many
     * blanks long, and short enough to be one step. */
    static constexpr uint32_t frames = 60u;
    /* How much larger the resized step makes the window. */
    static constexpr uint32_t resize_by = 32u;

    /* `images` keeps the draw and present steps' pictures. `window` is the
     * probe's own window, made by whoever runs it, and `windows` reads it back;
     * with no window the window-system steps do not run. */
    VulkanProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ImageSink &images, WindowFrame &windows,
                void *module, void *window);
    VulkanProbe(const VulkanProbe &) = delete;
    VulkanProbe &operator=(const VulkanProbe &) = delete;

    void run();

private:
    /* The avgpu adapter's LUID into `adapter_luid_`. */
    [[nodiscard]] Status adapter_step();
    [[nodiscard]] Status physical_device_step(ProbeDevice &device);
    [[nodiscard]] Status device_step(ProbeDevice &device);
    void                 lost_step(ProbeDevice &device);
    void                 recreated_step();
    /* The window-system steps, on a device of their own: a swapchain lives as
     * long as the device it was made on. */
    void                 window_steps(ProbeDevice &device);
    bool                 surface_step(PresentWork &present);
    bool                 present_step(PresentWork &present);
    void                 pacing_step();
    void                 resized_step(PresentWork &present);

    Kmt          &kmt_;
    VblankClock   clock_;
    VulkanEntry  &vulkan_;
    ImageSink    &images_;
    WindowFrame  &windows_;
    void         *module_;
    void         *window_;
    ScenarioSteps steps_;
    uint64_t      adapter_luid_;
    /* The performance-counter ticks the present step's frames took. */
    uint64_t      elapsed_;
};

}  // namespace av

#endif  // AV_VULKAN_PROBE_HPP
