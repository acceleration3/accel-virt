/* A client allocation is made from a valid descriptor and refused from one with a bad magic.
 * Every object is destroyed in reverse, and the teardown reported, whatever
 * failed. */
#ifndef AV_ALLOCATION_SCENARIO_HPP
#define AV_ALLOCATION_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class AllocationScenario {
public:
    AllocationScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

private:
    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
};

}  // namespace av

#endif  // AV_ALLOCATION_SCENARIO_HPP
