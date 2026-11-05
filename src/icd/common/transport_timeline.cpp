#include "icd/common/transport_timeline.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/host_channel.hpp"

namespace av::icd {

TransportTimeline::TransportTimeline(HostChannel &channel) : channel_(channel), waited_(0u)
{
    AV_SCOPE("TransportTimeline::TransportTimeline");
}

uint64_t TransportTimeline::recording() const
{
    AV_SCOPE("TransportTimeline::recording");
    return channel_.transport().published_fence() + 1u;
}

uint64_t TransportTimeline::covering() const
{
    AV_SCOPE("TransportTimeline::covering");
    return channel_.transport().covering_fence();
}

uint64_t TransportTimeline::completed() const
{
    AV_SCOPE("TransportTimeline::completed");
    const uint64_t shown = channel_.transport().gpu_completed();
    const uint64_t waited = __atomic_load_n(&waited_, __ATOMIC_ACQUIRE);
    return shown > waited ? shown : waited;
}

bool TransportTimeline::wait(uint64_t fence)
{
    AV_SCOPE("TransportTimeline::wait");
    uint64_t target = 0u;
    return publish(fence, target) && await(target);
}

bool TransportTimeline::publish(uint64_t fence, uint64_t &target)
{
    AV_SCOPE("TransportTimeline::publish");
    KmtTransport &transport = channel_.transport();
    if (fence > transport.published_fence() && transport.flush() != TransportResult::ok) return false;
    const uint64_t published = transport.published_fence();
    target = fence < published ? fence : published;
    return true;
}

bool TransportTimeline::await(uint64_t target)
{
    AV_SCOPE("TransportTimeline::await");
    if (channel_.transport().await_gpu(target) != Status::ok) return false;
    if (target > __atomic_load_n(&waited_, __ATOMIC_ACQUIRE)) __atomic_store_n(&waited_, target, __ATOMIC_RELEASE);
    return true;
}

void TransportTimeline::forget()
{
    AV_SCOPE("TransportTimeline::forget");
    __atomic_store_n(&waited_, 0u, __ATOMIC_RELEASE);
}

}
