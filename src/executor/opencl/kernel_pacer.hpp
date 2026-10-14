/* How many of a guest's kernels go to the host GPU in one submission.
 *
 * Rusticl gathers what a queue was given until it is flushed and submits it
 * as one job, and the host kernel resets a job that runs past its timeout -
 * amdgpu's is two seconds for compute - and loses the context, which Rusticl
 * answers by aborting the process: QEMU. An application that enqueues a few
 * hundred kernels before it waits makes such a job on a slow GPU: nbody's 400
 * steps of 17 ms each on a Radeon 610M were one job of seven seconds, natively
 * too. So the executor flushes after a group of kernels, and sizes the group
 * from how long groups take: one kernel at a time until a group has been timed,
 * then as many as fit `budget_ns`, never more than `most` - a fast GPU running
 * small kernels pays a flush per `most` of them and no more.
 *
 * A group is timed by a marker enqueued after it, from its flush until the
 * marker is first seen complete, which also counts any queue ahead of it: an
 * estimate that errs toward smaller groups, never toward longer jobs. One
 * kernel longer than the timeout is the application's, and is one job
 * whatever is done here. */
#ifndef AV_KERNEL_PACER_HPP
#define AV_KERNEL_PACER_HPP

#include <stdint.h>

namespace av {

struct KernelPacerProbe;

class KernelPacer {
public:
    /* A tenth of amdgpu's compute timeout: a group timed while the GPU was
     * idle may run slower under a guest's own load on another queue. */
    static constexpr uint64_t budget_ns = 200000000u;
    static constexpr uint32_t most = 64u;

    KernelPacer();

    /* A kernel was enqueued: whether its group is full, and the queue is to
     * be flushed now. */
    bool     kernel();
    /* The queue was flushed - by the guest, or because a group was full -
     * and what was gathered went with it. */
    void     flushed();
    /* Whether a group is being timed: one at a time. */
    bool     timing() const;
    /* The group just flushed at `now` is timed from here, by its marker. */
    void     time_from(uint64_t now);
    /* Its marker completed, seen at `now`: the size of the groups to come. */
    void     timed(uint64_t now);
    uint32_t group() const;

    bool invariants_hold() const;

private:
    uint32_t group_;
    uint32_t gathered_;
    /* The kernels of the group being timed, 0 when none is. */
    uint32_t timed_kernels_;
    uint64_t timed_from_;

    friend struct KernelPacerProbe;
};

}  // namespace av

#endif  // AV_KERNEL_PACER_HPP
