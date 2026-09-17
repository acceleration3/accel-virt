/* The probe's present step (feature 046): a surface on the probe's window, a
 * swapchain of the window's size, and the draw step's triangle rendered into
 * each acquired image and presented. What it judges is the window itself, read
 * back through WindowFrame - the pixels the desktop shows, not anything the
 * driver says about them. */
#ifndef AV_PRESENT_WORK_HPP
#define AV_PRESENT_WORK_HPP

#include "guest/suite/draw_work.hpp"

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
    /* Two images: what a synchronous present needs, and what the surface's
     * minimum offers. */
    static constexpr uint32_t images = 2u;
    /* The window's side, and the largest one a resize makes. */
    static constexpr uint32_t side = DrawWork::extent;
    static constexpr uint32_t max_side = 2u * side;
    static constexpr uint32_t max_frame_bytes = max_side * max_side * 4u;
    /* A present that has not come back in this long is a lost device. */
    static constexpr uint64_t acquire_timeout_ns = 5000000000ull;
    /* solid.frag's colour as a window's DIB holds it: blue, green, red. */
    static constexpr uint8_t window_colour[3] = {DrawWork::colour[2], DrawWork::colour[1], DrawWork::colour[0]};

    PresentWork(ProbeDevice &device, WindowFrame &windows, ImageSink &pictures, void *module, void *window);
    ~PresentWork();
    PresentWork(const PresentWork &) = delete;
    PresentWork &operator=(const PresentWork &) = delete;

    /* The surface for the window, and what it offers: whether the window's own
     * size is the current extent, the format it presents is offered, and both
     * present modes are. */
    bool     make_surface(ProbeOutcome &outcome);
    /* A swapchain of the window's current size in `mode`, with its render pass,
     * pipeline and one framebuffer per image. */
    bool     make_swapchain(VkPresentModeKHR mode, ProbeOutcome &outcome);
    /* `count` frames drawn and presented; the first result that was not
     * VK_SUCCESS, so a resize is the caller's to act on. */
    VkResult present_frames(uint32_t count, ProbeOutcome &outcome);
    /* The window read back and kept as `name`: how many of its pixels are the
     * triangle's colour. */
    uint32_t coloured_on_the_window(const char *name, ProbeOutcome &outcome);
    /* Everything of the swapchain, so another may be made over the same
     * surface - what an application does when its window is resized. */
    void     release_swapchain();

    uint32_t width() const;
    uint32_t height() const;
    uint32_t presented() const;

    /* A swapchain only over a surface, a framebuffer for each of its images,
     * and never more images than there is room for. */
    bool invariants_hold() const;

private:
    bool     make_render_pass(ProbeOutcome &outcome);
    bool     make_shaders(ProbeOutcome &outcome);
    bool     make_pipeline(ProbeOutcome &outcome);
    bool     make_framebuffers(ProbeOutcome &outcome);
    VkResult draw_and_present(uint32_t index, ProbeOutcome &outcome);
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
    VkShaderModule   vertex_;
    VkShaderModule   fragment_;
    VkPipelineLayout layout_;
    VkPipeline       pipeline_;
    uint32_t         presented_;
    uint8_t          frame_[max_frame_bytes];

    friend struct PresentWorkProbe;
};

}  // namespace av

#endif  // AV_PRESENT_WORK_HPP
