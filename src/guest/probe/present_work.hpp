#ifndef AV_PRESENT_WORK_HPP
#define AV_PRESENT_WORK_HPP

#include "guest/probe/draw_work.hpp"
#include "guest/probe/triangle_pipeline.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ImageSink;
class ProbeDevice;
class ProbeOutcome;
class WindowFrame;
struct PresentWorkProbe;

class PresentWork {
public:
    static constexpr uint32_t images = 2u;
    static constexpr uint32_t side = DrawWork::extent;
    static constexpr uint32_t max_side = 2u * side;
    static constexpr uint32_t max_frame_bytes = max_side * max_side * 4u;
    static constexpr uint64_t acquire_timeout_ns = 5000000000ull;
    static constexpr uint8_t window_colour[3] = {DrawWork::colour[2], DrawWork::colour[1], DrawWork::colour[0]};
    static constexpr uint8_t background[4] = {192u, 32u, 96u, 255u};
    static constexpr uint8_t window_background[3] = {background[2], background[1], background[0]};

    PresentWork(ProbeDevice &device, WindowFrame &windows, ImageSink &pictures, void *module, void *window);
    ~PresentWork();
    PresentWork(const PresentWork &) = delete;
    PresentWork &operator=(const PresentWork &) = delete;

    bool     make_surface(ProbeOutcome &outcome);
    bool     make_swapchain(VkPresentModeKHR mode, ProbeOutcome &outcome);
    VkResult present_frames(uint32_t count, ProbeOutcome &outcome);
    uint32_t drawn_on_the_window(const char *name, ProbeOutcome &outcome);
    void     release_swapchain();

    uint32_t width() const;
    uint32_t height() const;
    uint32_t presented() const;

    bool invariants_hold() const;

private:
    bool     make_render_pass(ProbeOutcome &outcome);
    bool     make_framebuffers(ProbeOutcome &outcome);
    VkResult draw_and_present(uint32_t index, ProbeOutcome &outcome);
    uint32_t matched_frame() const;
    void     release_pipeline();

    ProbeDevice     &device_;
    WindowFrame     &windows_;
    ImageSink       &pictures_;
    void            *module_;
    void            *window_;
    VkSurfaceKHR     surface_;
    VkSwapchainKHR   swapchain_;
    VkFormat         format_;
    uint32_t         width_;
    uint32_t         height_;
    VkImage          images_[images];
    VkImageView      views_[images];
    VkFramebuffer    framebuffers_[images];
    uint32_t         count_;
    VkRenderPass     render_pass_;
    TrianglePipeline triangle_;
    uint32_t         presented_;
    uint8_t          frame_[max_frame_bytes];

    friend struct PresentWorkProbe;
};

}

#endif
