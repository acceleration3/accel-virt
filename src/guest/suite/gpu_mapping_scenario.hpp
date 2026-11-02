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

}

#endif
