#ifndef AV_PROBE_COMMANDS_HPP
#define AV_PROBE_COMMANDS_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ProbeDevice;
class ProbeOutcome;
struct ProbeCommandsProbe;

class ProbeCommands {
public:
    static constexpr uint64_t wait_nanoseconds = 10000000000u;

    explicit ProbeCommands(ProbeDevice &device);
    ~ProbeCommands();
    ProbeCommands(const ProbeCommands &) = delete;
    ProbeCommands &operator=(const ProbeCommands &) = delete;

    bool begin(ProbeOutcome &outcome);
    bool run(ProbeOutcome &outcome);

    VkCommandBuffer buffer() const;

    bool invariants_hold() const;

private:
    ProbeDevice    &device_;
    VkCommandPool   pool_;
    VkCommandBuffer buffer_;
    VkFence         fence_;

    friend struct ProbeCommandsProbe;
};

}

#endif
