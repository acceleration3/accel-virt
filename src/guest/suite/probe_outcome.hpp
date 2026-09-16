/* How one of the probe's workloads went (feature 045, T037): the first Vulkan
 * call that failed, or that no memory type would do, or how many of the
 * values read back were right - and the report line that says so. */
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

    /* Keeps `result` when it is the first failure: whether it succeeded. */
    bool accept(VkResult result);
    /* Nothing the device offers would do. */
    void missing();
    /* `matched` values of `expected` were right. */
    void measure(uint32_t matched, uint32_t expected);
    /* A failed call's result; not_found with 0 for something missing;
     * otherwise the count, passing only when all were right. */
    void report(ScenarioSteps &steps, const char *step) const;

    /* A failure is one thing - never a failed call and a missing type at once -
     * and no more values are right than were expected. */
    bool invariants_hold() const;

private:
    VkResult result_;
    bool     missing_;
    uint32_t matched_;
    uint32_t expected_;

    friend struct ProbeOutcomeProbe;
};

}  // namespace av

#endif  // AV_PROBE_OUTCOME_HPP
