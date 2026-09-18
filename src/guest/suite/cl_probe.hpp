/* What rvkclprobe.exe proves, through whatever OpenCL it was given.
 *
 * In the guest that is the Khronos loader beside it, which is the point: what
 * SC-001 is about is the loader's own discovery of this driver through
 * `OpenCLDriverName` on the rvkgpu adapter, and nothing but a real loader
 * proves that. On the full-stack rig the same steps run against the ICD's
 * dispatch table reached the way a loader reaches it.
 *
 * Each step is one line of the report the runner reads. */
#ifndef AV_CL_PROBE_HPP
#define AV_CL_PROBE_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class ClEntry;
class GuestReport;
class Kmt;

class ClProbe {
public:
    /* The vector add every step builds on: small enough to read back inline,
     * large enough that a kernel that ran on nothing shows. */
    static constexpr uint32_t elements = 64u;

    ClProbe(Kmt &kmt, GuestReport &report, ClEntry &cl);
    ClProbe(const ClProbe &) = delete;
    ClProbe &operator=(const ClProbe &) = delete;

    void run();

private:
    /* The platform this driver's loader found, and that it is ours. */
    [[nodiscard]] Status platform_step();
    /* One device, and what it says it is. */
    [[nodiscard]] Status device_step();
    /* The adapter's own LUID rather than the host GPU's. */
    void                 luid_step();
    [[nodiscard]] Status context_step();
    [[nodiscard]] Status queue_step();
    [[nodiscard]] Status program_step();
    [[nodiscard]] Status kernel_step();
    /* The whole point: a kernel that ran on the host GPU, read back and
     * compared with what the inputs add to. */
    [[nodiscard]] Status read_step();
    /* A mapping of a buffer, written through and seen by the device. */
    void                 map_step();
    /* Shared memory a kernel takes as an argument. */
    void                 svm_step();
    /* A callback that runs once, on a thread the ICD owns. */
    void                 callback_step();
    void                 release_all();

    ScenarioSteps steps_;
    GuestReport  &report_;
    ClEntry      &cl_;
    void         *platform_;
    void         *device_;
    void         *context_;
    void         *queue_;
    void         *program_;
    void         *kernel_;
    void         *buffers_[3];
    /* What the inputs hold and what their sum must be. */
    float         first_[elements];
    float         second_[elements];
    float         sum_[elements];
};

}  // namespace av

#endif  // AV_CL_PROBE_HPP
