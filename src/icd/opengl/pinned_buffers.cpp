#include "icd/opengl/pinned_buffers.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "icd/common/host_channel.hpp"
#include "proto/resources/host_memory_request.hpp"

namespace av::glicd {

PinnedBuffers::PinnedBuffers(BlockSource &memory, icd::HostChannel &channel)
    : channel_(channel), bound_(0u), pinned_(memory, Pool::paged)
{
    AV_SCOPE("PinnedBuffers::PinnedBuffers");
}

void PinnedBuffers::bind(uint32_t buffer)
{
    AV_SCOPE("PinnedBuffers::bind");
    bound_ = buffer;
}

uint32_t PinnedBuffers::bound() const
{
    AV_SCOPE("PinnedBuffers::bound");
    return bound_;
}

/* The whole pages under the bytes are lent: the device lends whole pages. */
Status PinnedBuffers::pin(const void *address, uint64_t bytes, HostAllocation *&lent, uint64_t &offset)
{
    AV_SCOPE("PinnedBuffers::pin");
    lent = nullptr;
    offset = 0u;
    if (bound_ == 0u) return Status::invalid_state;
    if (pinned_.find(bound_) != nullptr) return Status::conflict;
    if (pinned_.reserve(pinned_.count() + 1u) != Status::ok) return Status::no_memory;
    const uintptr_t at = reinterpret_cast<uintptr_t>(address);
    const uintptr_t first = at & ~uintptr_t{page_bytes - 1u};
    const uintptr_t end = (at + bytes + page_bytes - 1u) & ~uintptr_t{page_bytes - 1u};
    const Status    lent_status = channel_.adopt_memory(reinterpret_cast<void *>(first), end - first,
                                                        HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u), lent);
    if (lent_status != Status::ok) return lent_status;
    uint32_t slot = 0u;
    static_cast<void>(pinned_.insert(bound_, lent, slot));
    offset = at - first;
    return Status::ok;
}

void PinnedBuffers::forget(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("PinnedBuffers::forget");
    for (int32_t index = 0; buffers != nullptr && index < count; index++) {
        HostAllocation *lent = nullptr;
        if (pinned_.take(buffers[index], lent)) channel_.retire_memory(*lent);
        if (buffers[index] == bound_) bound_ = 0u;
    }
}

void PinnedBuffers::reclaim()
{
    AV_SCOPE("PinnedBuffers::reclaim");
    channel_.reclaim_retired();
}

}  // namespace av::glicd
