/* A command buffer mapped at a GPU virtual address is submitted on a context, and the context's completion awaited.
 * Every object is destroyed in reverse, and the teardown reported, whatever
 * failed. */
#ifndef AV_SUBMISSION_SCENARIO_HPP
#define AV_SUBMISSION_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class SubmissionScenario {
public:
    SubmissionScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

private:
    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
};

}  // namespace av

#endif  // AV_SUBMISSION_SCENARIO_HPP
