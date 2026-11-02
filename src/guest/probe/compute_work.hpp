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
    static constexpr uint32_t local_size = 16u;

    explicit ComputeWork(ProbeDevice &device);
    ~ComputeWork();
    ComputeWork(const ComputeWork &) = delete;
    ComputeWork &operator=(const ComputeWork &) = delete;

    void run(ProbeOutcome &outcome);

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

}

#endif
