#include "icd/d3d/stream_timeline.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/d3d/d3d_command_stream.hpp"

namespace av::d3d {

StreamTimeline::StreamTimeline(D3dCommandStream &stream, KmtTransport &transport, ThreadLock &lock)
    : stream_(stream), transport_(transport), lock_(lock), waited_(0u)
{
    AV_SCOPE("StreamTimeline::StreamTimeline");
}

uint64_t StreamTimeline::recording() const
{
    AV_SCOPE("StreamTimeline::recording");
    return transport_.published_fence() + 1u;
}

uint64_t StreamTimeline::completed() const
{
    AV_SCOPE("StreamTimeline::completed");
    const uint64_t shown = transport_.gpu_completed();
    const uint64_t waited = __atomic_load_n(&waited_, __ATOMIC_ACQUIRE);
    return shown > waited ? shown : waited;
}

/* A batch that went out with nothing recorded in it reads nothing, so a fence
 * still unpublished once the stream is sent is waited for as the newest that
 * was. */
bool StreamTimeline::wait(uint64_t fence)
{
    AV_SCOPE("StreamTimeline::wait");
    const ThreadLockHold release(lock_);
    if (fence > transport_.published_fence() && stream_.flush() != Status::ok) return false;
    const uint64_t published = transport_.published_fence();
    const uint64_t target = fence < published ? fence : published;
    if (transport_.await_gpu(target) != Status::ok) return false;
    if (target > __atomic_load_n(&waited_, __ATOMIC_ACQUIRE)) __atomic_store_n(&waited_, target, __ATOMIC_RELEASE);
    return true;
}

}  // namespace av::d3d
