#ifndef AV_GUEST_SUITE_HPP
#define AV_GUEST_SUITE_HPP

#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/suite/allocation_scenario.hpp"
#include "guest/suite/fence_stress_scenario.hpp"
#include "guest/suite/gpu_mapping_scenario.hpp"
#include "guest/suite/host_loss_scenario.hpp"
#include "guest/suite/host_visible_import_scenario.hpp"
#include "guest/suite/isolation_scenario.hpp"
#include "guest/suite/opencl_scenario.hpp"
#include "guest/suite/submission_scenario.hpp"
#include "guest/suite/submit_benchmark.hpp"
#include "guest/suite/timeout_scenario.hpp"
#include "guest/suite/transport_scenario.hpp"
#include "guest/suite/vsync_scenario.hpp"
#include "guest/suite/vulkan_scenario.hpp"

#include <stdint.h>

namespace av {

class GuestReport;
class Kmt;

class GuestSuite {
public:
    GuestSuite(Kmt &kmt, GuestReport &report, uint32_t stress_seconds);
    GuestSuite(const GuestSuite &) = delete;
    GuestSuite &operator=(const GuestSuite &) = delete;

    uint32_t run();
    uint32_t run_timeout();
    uint32_t run_round_trip();
    uint32_t run_av();
    uint32_t run_host_loss();

private:
    Kmt               &kmt_;
    GuestReport       &report_;
    AdapterSearch      search_;
    AdapterScenario    adapter_;
    AllocationScenario allocation_;
    GpuMappingScenario mapping_;
    SubmissionScenario submission_;
    VsyncScenario       vsync_;
    TransportScenario   transport_;
    FenceStressScenario stress_;
    TimeoutScenario     timeout_;
    TransportScenario   opengl_;
    VulkanScenario      vulkan_;
    OpenClScenario      opencl_;
    HostVisibleImportScenario host_import_;
    IsolationScenario   isolation_;
    HostLossScenario    host_loss_;
    SubmitBenchmark     submit_;
};

}

#endif
