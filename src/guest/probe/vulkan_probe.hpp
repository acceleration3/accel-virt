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
    static constexpr uint32_t frames = 60u;
    static constexpr uint32_t resize_by = 32u;

    VulkanProbe(Kmt &kmt, GuestReport &report, VulkanEntry &vulkan, ImageSink &images, WindowFrame &windows,
                void *module, void *window);
    VulkanProbe(const VulkanProbe &) = delete;
    VulkanProbe &operator=(const VulkanProbe &) = delete;

    void run();

private:
    [[nodiscard]] Status adapter_step();
    [[nodiscard]] Status physical_device_step(ProbeDevice &device);
    [[nodiscard]] Status device_step(ProbeDevice &device);
    void                 lost_step(ProbeDevice &device);
    void                 recreated_step();
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
    uint64_t      elapsed_;
};

}

#endif
