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

    bool       make(ProbeOutcome &outcome, VkRenderPass pass, uint32_t width, uint32_t height);
    VkPipeline pipeline() const;
    void       release();

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

}

#endif
