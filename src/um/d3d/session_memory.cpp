#include "um/d3d/session_memory.hpp"

#include "core/log/driver_log.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "icd/common/host_channel.hpp"
#include "um/d3d/slice_offer.hpp"

#include <windows.h>

namespace av::d3d {

SessionMemory::SessionMemory(BlockSource &memory, icd::HostChannel &channel, ThreadLock &channel_lock,
                             DriverLog &lines, const SliceOffer *offer, const void *owner)
    : memory_(memory), channel_(channel), channel_lock_(channel_lock), lines_(lines), offer_(offer), owner_(owner),
      slices_lock_(), timeline_(), streams_{}, lent_pages_(memory, slices_lock_), clock_()
{
    AV_SCOPE("SessionMemory::SessionMemory");
}

void SessionMemory::open(ClientTransport &host)
{
    AV_SCOPE("SessionMemory::open");
    if (host_grants_slices(host)) static_cast<void>(timeline_.construct(channel_, channel_lock_));
}

bool SessionMemory::host_grants_slices(ClientTransport &host)
{
    AV_SCOPE("SessionMemory::host_grants_slices");
    if (offer_ == nullptr || !channel_.has_arena()) return false;
    return offer_->granted(host, lines_);
}

void SessionMemory::close_streams()
{
    AV_SCOPE("SessionMemory::close_streams");
    for (ObjectStorage<icd::StreamMemory> &stream : streams_) stream.destroy();
    if (timeline_.constructed()) timeline_.destroy();
}

void SessionMemory::release_pages()
{
    AV_SCOPE("SessionMemory::release_pages");
    lent_pages_.release_all();
}

icd::StreamMemory *SessionMemory::stream(HostMemoryRequest::Kind kind)
{
    AV_SCOPE("SessionMemory::stream");
    if (!timeline_.constructed() || (kind != HostMemoryRequest::Kind::upload && kind != HostMemoryRequest::Kind::readback))
        return nullptr;
    ObjectStorage<icd::StreamMemory> &made = streams_[static_cast<uint32_t>(kind) - 1u];
    const ThreadLockHold              held(slices_lock_);
    if (!made.constructed() &&
        made.construct(memory_, channel_, owner_, timeline_.object(), slices_lock_, clock_, lines_, kind,
                       offer_->alignment()) != Status::ok)
        return nullptr;
    return &made.object();
}

void SessionMemory::retire_memory(HostAllocation &allocation)
{
    AV_SCOPE("SessionMemory::retire_memory");
    if (!timeline_.constructed()) {
        static_cast<void>(channel_.close_memory(allocation));
        return;
    }
    const uint64_t fence = timeline_.object().recording();
    while (channel_.retire_memory_after(allocation, fence) != Status::ok) {
        HostAllocation *oldest = nullptr;
        uint64_t        oldest_fence = fence;
        const bool      waited = channel_.take_oldest_retired(oldest, oldest_fence);
        static_cast<void>(timeline_.object().wait(oldest_fence));
        static_cast<void>(channel_.close_memory(waited ? *oldest : allocation));
        if (!waited) return;
    }
}

void SessionMemory::retire_pages(void *pages)
{
    AV_SCOPE("SessionMemory::retire_pages");
    if (!timeline_.constructed()) {
        static_cast<void>(VirtualFree(pages, 0u, MEM_RELEASE));
        return;
    }
    const uint64_t fence = timeline_.object().recording();
    if (lent_pages_.retire(pages, fence) == Status::ok) return;
    static_cast<void>(timeline_.object().wait(fence));
    static_cast<void>(VirtualFree(pages, 0u, MEM_RELEASE));
}

void SessionMemory::reclaim()
{
    AV_SCOPE("SessionMemory::reclaim");
    if (!timeline_.constructed()) return;
    const uint64_t completed = timeline_.object().completed();
    channel_.reclaim_retired_through(completed);
    lent_pages_.reclaim(completed);
}

uint64_t SessionMemory::gpu_fence() const
{
    AV_SCOPE("SessionMemory::gpu_fence");
    return timeline_.constructed() ? timeline_.object().covering() : 0u;
}

bool SessionMemory::gpu_pending(uint64_t fence) const
{
    AV_SCOPE("SessionMemory::gpu_pending");
    if (fence == 0u || !timeline_.constructed() || !channel_.transport().knows_gpu_progress()) return false;
    return timeline_.object().completed() < fence;
}

void SessionMemory::trim()
{
    AV_SCOPE("SessionMemory::trim");
    for (ObjectStorage<icd::StreamMemory> &made : streams_) {
        icd::StreamMemory *trimmed = nullptr;
        {
            const ThreadLockHold held(slices_lock_);
            if (made.constructed()) trimmed = &made.object();
        }
        if (trimmed != nullptr) trimmed->trim();
    }
}

bool SessionMemory::invariants_hold() const
{
    AV_SCOPE("SessionMemory::invariants_hold");
    if (!lent_pages_.invariants_hold()) return false;
    for (const ObjectStorage<icd::StreamMemory> &made : streams_)
        if (made.constructed() && !made.object().invariants_hold()) return false;
    return true;
}

}
