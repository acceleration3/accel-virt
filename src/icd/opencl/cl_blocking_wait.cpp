#include "icd/opencl/cl_blocking_wait.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/host_completion_wait.hpp"
#include "icd/opencl/cl_events_probe.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClBlockingWait::ClBlockingWait(Kmt &kmt, ClEventWatcher &watcher)
    : kmt_(kmt), watcher_(watcher), rounds_(0u), raises_asked_(0u)
{
    AV_SCOPE("ClBlockingWait::ClBlockingWait");
}

cl_int ClBlockingWait::until_complete(ClientTransport &wire, const uint64_t *events, uint32_t count,
                                      uint64_t timeout, bool driven)
{
    AV_SCOPE("ClBlockingWait::until_complete");
    if (events == nullptr || count == 0u) return CL_INVALID_VALUE;
    ClEventsProbe            probe(wire, kmt_, watcher_, events, count, driven, rounds_, raises_asked_);
    icd::HostCompletionWait wait(kmt_, timeout);
    return wait.run(probe) ? probe.answer() : CL_SUCCESS;
}

cl_int ClBlockingWait::until_queued_completes(ClientTransport &wire, uint64_t queue,
                                              uint64_t event, bool driven)
{
    AV_SCOPE("ClBlockingWait::until_queued_completes");
    if (queue == 0u || event == 0u) return CL_INVALID_VALUE;
    const cl_int flushed = av::clproto::av_clFlush(wire, queue);
    if (flushed != CL_SUCCESS) return flushed;
    return until_complete(wire, &event, 1u, icd::SlicedWait::forever, driven);
}

uint64_t ClBlockingWait::rounds() const
{
    AV_SCOPE("ClBlockingWait::rounds");
    return rounds_;
}

uint64_t ClBlockingWait::raises_asked() const
{
    AV_SCOPE("ClBlockingWait::raises_asked");
    return raises_asked_;
}

}
