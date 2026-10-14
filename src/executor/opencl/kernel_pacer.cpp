#include "executor/opencl/kernel_pacer.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

KernelPacer::KernelPacer() : group_(1u), gathered_(0u), timed_kernels_(0u), timed_from_(0u)
{
    AV_SCOPE("KernelPacer::KernelPacer");
}

bool KernelPacer::kernel()
{
    AV_SCOPE("KernelPacer::kernel");
    gathered_++;
    return gathered_ >= group_;
}

void KernelPacer::flushed()
{
    AV_SCOPE("KernelPacer::flushed");
    gathered_ = 0u;
}

bool KernelPacer::timing() const
{
    AV_SCOPE("KernelPacer::timing");
    return timed_kernels_ != 0u;
}

void KernelPacer::time_from(uint64_t now)
{
    AV_SCOPE("KernelPacer::time_from");
    timed_kernels_ = group_;
    timed_from_ = now;
}

void KernelPacer::timed(uint64_t now)
{
    AV_SCOPE("KernelPacer::timed");
    if (timed_kernels_ == 0u) return;
    const uint64_t took = now > timed_from_ ? now - timed_from_ : 0u;
    const uint64_t each = took / timed_kernels_;
    const uint64_t fits = each != 0u ? budget_ns / each : most;
    group_ = fits < 1u ? 1u : fits > most ? most : static_cast<uint32_t>(fits);
    timed_kernels_ = 0u;
}

uint32_t KernelPacer::group() const
{
    AV_SCOPE("KernelPacer::group");
    return group_;
}

/* A group is one to `most` kernels, and no more are gathered than the
 * largest holds. */
bool KernelPacer::invariants_hold() const
{
    AV_SCOPE("KernelPacer::invariants_hold");
    return group_ >= 1u && group_ <= most && gathered_ <= most && timed_kernels_ <= most;
}

}  // namespace av
