#ifndef AV_PROBE_OUTCOME_HPP
#define AV_PROBE_OUTCOME_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class ScenarioSteps;
struct ProbeOutcomeProbe;

class ProbeOutcome {
public:
    ProbeOutcome();
    ProbeOutcome(const ProbeOutcome &) = delete;
    ProbeOutcome &operator=(const ProbeOutcome &) = delete;

    bool accept(VkResult result);
    void missing();
    void measure(uint32_t matched, uint32_t expected);
    void report(ScenarioSteps &steps, const char *step) const;

    bool invariants_hold() const;

private:
    VkResult result_;
    bool     missing_;
    uint32_t matched_;
    uint32_t expected_;

    friend struct ProbeOutcomeProbe;
};

}

#endif
