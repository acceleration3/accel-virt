/* A client allocation is made resident and mapped at a GPU virtual address, each paging fence reached.
 * Every object is destroyed in reverse, and the teardown reported, whatever
 * failed. */
#ifndef AV_GPU_MAPPING_SCENARIO_HPP
#define AV_GPU_MAPPING_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class GpuMappingScenario {
public:
    GpuMappingScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

private:
    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
};

}  // namespace av

#endif  // AV_GPU_MAPPING_SCENARIO_HPP
