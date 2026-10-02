/* The probes' one graphics pipeline: the triangle shader and the solid
 * fragment shader, drawn with no vertex input into a render pass's single
 * colour attachment at an extent of the caller's - an image of the probe's
 * own (DrawWork) or a swapchain's (PresentWork). */
#ifndef AV_GUEST_PROBE_TRIANGLE_PIPELINE_HPP
#define AV_GUEST_PROBE_TRIANGLE_PIPELINE_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ProbeDevice;
class ProbeOutcome;
struct TrianglePipelineProbe;

class TrianglePipeline {
public:
    explicit TrianglePipeline(ProbeDevice &device);
    ~TrianglePipeline();
    TrianglePipeline(const TrianglePipeline &) = delete;
    TrianglePipeline &operator=(const TrianglePipeline &) = delete;

    /* The shaders, layout and pipeline for `pass` at width x height; false,
     * with the reason in `outcome`, at the first that the device refuses. */
    bool       make(ProbeOutcome &outcome, VkRenderPass pass, uint32_t width, uint32_t height);
    VkPipeline pipeline() const;
    /* Everything made, destroyed: Vulkan makes destroying VK_NULL_HANDLE a
     * no-op, so this is safe on a pipeline never made or already released. */
    void       release();

    /* A pipeline is only ever made over a layout and both shaders. */
    bool invariants_hold() const;

private:
    friend struct TrianglePipelineProbe;

    bool make_shaders(ProbeOutcome &outcome);

    ProbeDevice     &device_;
    VkShaderModule   vertex_;
    VkShaderModule   fragment_;
    VkPipelineLayout layout_;
    VkPipeline       pipeline_;
};

}  // namespace av

#endif  // AV_GUEST_PROBE_TRIANGLE_PIPELINE_HPP
