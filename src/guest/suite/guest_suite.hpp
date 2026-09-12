/* The in-guest suite: every scenario in order against the one adapter the
 * first finds, then the adapter closed and the report finished. */
#ifndef AV_GUEST_SUITE_HPP
#define AV_GUEST_SUITE_HPP

#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/allocation_scenario.hpp"
#include "guest/suite/gpu_mapping_scenario.hpp"
#include "guest/suite/submission_scenario.hpp"
#include "guest/suite/vsync_scenario.hpp"

#include <stdint.h>

namespace av {

class GuestReport;
class Kmt;

class GuestSuite {
public:
    GuestSuite(Kmt &kmt, GuestReport &report);
    GuestSuite(const GuestSuite &) = delete;
    GuestSuite &operator=(const GuestSuite &) = delete;

    /* The failures reported. */
    uint32_t run();

private:
    Kmt               &kmt_;
    GuestReport       &report_;
    AdapterSearch      search_;
    AdapterScenario    adapter_;
    AllocationScenario allocation_;
    GpuMappingScenario mapping_;
    SubmissionScenario submission_;
    VsyncScenario      vsync_;
};

}  // namespace av

#endif  // AV_GUEST_SUITE_HPP
