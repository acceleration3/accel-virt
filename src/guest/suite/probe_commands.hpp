/* One command buffer of the probe's, recorded once and run to completion
 * (feature 045, T037): its pool on the device's family, the buffer, and the
 * fence its submission signals, waited for as long as a slow host GPU could
 * take. The pool and fence go when this does, and the buffer with its pool. */
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
    /* Ten seconds: far past any work here, and still an end to a lost one. */
    static constexpr uint64_t wait_nanoseconds = 10000000000u;

    explicit ProbeCommands(ProbeDevice &device);
    ~ProbeCommands();
    ProbeCommands(const ProbeCommands &) = delete;
    ProbeCommands &operator=(const ProbeCommands &) = delete;

    /* Makes the pool, buffer and fence and begins recording: whether it is
     * recording, with `outcome` told why not. */
    bool begin(ProbeOutcome &outcome);
    /* Ends recording, submits with the fence and waits for it: whether the
     * work completed. */
    bool run(ProbeOutcome &outcome);

    /* Null until begun. */
    VkCommandBuffer buffer() const;

    /* A buffer or fence only with a pool. */
    bool invariants_hold() const;

private:
    ProbeDevice    &device_;
    VkCommandPool   pool_;
    VkCommandBuffer buffer_;
    VkFence         fence_;

    friend struct ProbeCommandsProbe;
};

}  // namespace av

#endif  // AV_PROBE_COMMANDS_HPP
