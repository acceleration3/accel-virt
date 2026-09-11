/* The virtio-gpu-rvk adapter is found and reports WDDM 3.2.
 * The adapter stays open in the search for the scenarios after it. */
#ifndef AV_ADAPTER_SCENARIO_HPP
#define AV_ADAPTER_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class AdapterScenario {
public:
    static constexpr uint32_t virtio_vendor = 0x1AF4u;
    static constexpr uint32_t virtio_gpu_device = 0x1050u;
    static constexpr uint32_t rvk_subsystem = 0x5256u;
    static constexpr uint32_t wddm_3_2 = 3200u;

    AdapterScenario(Kmt &kmt, GuestReport &report, AdapterSearch &search);

    void run();

private:
    Kmt           &kmt_;
    AdapterSearch &search_;
    ScenarioSteps  steps_;
};

}  // namespace av

#endif  // AV_ADAPTER_SCENARIO_HPP
