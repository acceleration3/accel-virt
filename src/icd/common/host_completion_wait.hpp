/* Drives one guest wait on host work to its end within a deadline, a slice at
 * a time (SlicedWait), timed on the guest's own performance counter read
 * through the Kmt. A host call that blocks holds its context's worker, and a
 * batch that does not complete within TdrDelay (2 s) has dxgkrnl reset the
 * adapter, so no wait - Vulkan's fences and semaphores, OpenCL's blocking
 * calls, OpenGL's syncs - is asked of the host whole: each asks its
 * CompletionProbe with a slice, pauses as the probe says, and asks again
 * until the probe says it is over or the deadline has passed. The first
 * slice is always asked, so a zero timeout still polls once; SlicedWait::
 * forever never passes. */
#ifndef AV_ICD_HOST_COMPLETION_WAIT_HPP
#define AV_ICD_HOST_COMPLETION_WAIT_HPP

#include "icd/common/sliced_wait.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}  // namespace av

namespace av::icd {

class CompletionProbe;

class HostCompletionWait {
public:
    /* A wait of `timeout` nanoseconds, from now. */
    HostCompletionWait(const Kmt &kmt, uint64_t timeout);
    HostCompletionWait(const HostCompletionWait &) = delete;
    HostCompletionWait &operator=(const HostCompletionWait &) = delete;

    /* True when `probe` said the wait is over, false when the deadline passed
     * first. A wait is run once. */
    [[nodiscard]] bool run(CompletionProbe &probe);

private:
    const Kmt &kmt_;
    uint64_t   frequency_;
    /* Read before the wait is made of it: the counter's own frequency is
     * written by the same read. */
    uint64_t   start_;
    SlicedWait wait_;
};

}  // namespace av::icd

#endif  // AV_ICD_HOST_COMPLETION_WAIT_HPP
