/* The probe's compute step (feature 045, T037): 256 unsigned integers written
 * through a mapped storage buffer, doubled in place by double.comp in one
 * dispatch, and counted back through the same mapping. */
#ifndef AV_COMPUTE_WORK_HPP
#define AV_COMPUTE_WORK_HPP

#include "guest/probe/probe_buffer.hpp"
#include "guest/probe/probe_commands.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ProbeDevice;
class ProbeOutcome;
struct ComputeWorkProbe;

class ComputeWork {
public:
    static constexpr uint32_t elements = 256u;
    /* double.comp's local_size_x. */
    static constexpr uint32_t local_size = 16u;

    explicit ComputeWork(ProbeDevice &device);
    ~ComputeWork();
    ComputeWork(const ComputeWork &) = delete;
    ComputeWork &operator=(const ComputeWork &) = delete;

    /* How many elements came back doubled, or why none could. */
    void run(ProbeOutcome &outcome);

    /* A pipeline only over its layout, a layout only over its set's, a set
     * only from its pool. */
    bool invariants_hold() const;

private:
    bool make_pipeline(ProbeOutcome &outcome);
    bool make_set(ProbeOutcome &outcome);
    uint32_t doubled() const;

    ProbeDevice          &device_;
    ProbeBuffer           elements_;
    ProbeCommands         commands_;
    VkShaderModule        shader_;
    VkDescriptorSetLayout set_layout_;
    VkPipelineLayout      layout_;
    VkPipeline            pipeline_;
    VkDescriptorPool      pool_;
    VkDescriptorSet       set_;

    friend struct ComputeWorkProbe;
};

}  // namespace av

#endif  // AV_COMPUTE_WORK_HPP
