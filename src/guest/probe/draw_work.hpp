/* The probe's draw step (feature 045, T037): triangle.vert's full-screen
 * triangle shaded by solid.frag into a 64x64 image, the image copied into a
 * mapped buffer in the same command buffer, and every pixel of the copy
 * compared with the shader's colour. */
#ifndef AV_DRAW_WORK_HPP
#define AV_DRAW_WORK_HPP

#include "guest/probe/probe_buffer.hpp"
#include "guest/probe/probe_commands.hpp"
#include "guest/probe/probe_image.hpp"
#include "guest/probe/triangle_pipeline.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ImageSink;
class ProbeDevice;
class ProbeOutcome;
struct DrawWorkProbe;

class DrawWork {
public:
    static constexpr uint32_t extent = 64u;
    static constexpr uint32_t pixels = extent * extent;
    /* solid.frag's vec4(0.25, 0.5, 0.75, 1.0) as UNORM bytes, each within
     * one of the rounding a driver may choose (127.5 is either neighbour). */
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    /* `images` keeps the picture the step judges, as vulkan-draw. */
    DrawWork(ProbeDevice &device, ImageSink &images);
    ~DrawWork();
    DrawWork(const DrawWork &) = delete;
    DrawWork &operator=(const DrawWork &) = delete;

    /* How many pixels came back the colour, or why none could. */
    void run(ProbeOutcome &outcome);

    /* A framebuffer only over its render pass, a pipeline only over its
     * layout. */
    bool invariants_hold() const;

private:
    bool make_render_pass(ProbeOutcome &outcome);
    void record();
    uint32_t coloured() const;

    ProbeDevice     &device_;
    ImageSink       &images_;
    ProbeImage       image_;
    ProbeBuffer      readback_;
    ProbeCommands    commands_;
    VkRenderPass     render_pass_;
    VkFramebuffer    framebuffer_;
    TrianglePipeline triangle_;

    friend struct DrawWorkProbe;
};

}  // namespace av

#endif  // AV_DRAW_WORK_HPP
