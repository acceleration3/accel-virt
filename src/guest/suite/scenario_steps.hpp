/* The steps of one named scenario as they reach the report: each outcome with
 * the NTSTATUS of the D3DKMT call behind it. */
#ifndef AV_SCENARIO_STEPS_HPP
#define AV_SCENARIO_STEPS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestReport;
class GuestSession;
class Kmt;

class ScenarioSteps {
public:
    ScenarioSteps(Kmt &kmt, GuestReport &report, const char *scenario);

    /* Records a step that must succeed: its status, to chain on. */
    Status check(const char *step, Status status);
    /* The same, with an NTSTATUS from somewhere other than this scenario's own
     * Kmt. A probe that calls a driver through an API of its own does not make
     * the kernel call itself - the driver does, in another module, with its
     * own Kmt - so the code it must report is the driver's and not this one's
     * (feature 053). */
    Status check_kernel(const char *step, Status status, uint32_t code);
    /* Records a step whose failure is explained by a measurement rather than
     * by an NTSTATUS: the value is reported where the code would be. */
    Status check_measured(const char *step, Status status, uint32_t measurement);
    /* Records a step judged by a graphics API's own result code, where 0 is
     * success for Vulkan and OpenCL alike: any other value fails the step and
     * is reported, as its two's-complement bits, where the NTSTATUS would be. */
    Status check_result(const char *step, int32_t result);
    /* Something a step learnt that is not a verdict: `note scenario.key
     * text`, which a reader of the report sees and the run is not failed for.
     * What a probe says when the driver has told it there is nothing here to
     * prove (feature 053). */
    void   note(const char *key, const char *text);
    /* Records a step the driver must refuse. */
    void   check_refused(const char *step, Status status);
    /* Tears the session down and records it with its first failure's NTSTATUS. */
    void   check_teardown(GuestSession &session);

private:
    Kmt         &kmt_;
    GuestReport &report_;
    const char  *scenario_;
};

}  // namespace av

#endif  // AV_SCENARIO_STEPS_HPP
