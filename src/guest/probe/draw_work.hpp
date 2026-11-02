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
    static constexpr uint8_t colour[4] = {64u, 128u, 191u, 255u};

    DrawWork(ProbeDevice &device, ImageSink &images);
    ~DrawWork();
    DrawWork(const DrawWork &) = delete;
    DrawWork &operator=(const DrawWork &) = delete;

    void run(ProbeOutcome &outcome);

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

}

#endif
