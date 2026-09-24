#include "um/d3d/query_results.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

namespace av::d3d11 {

QueryResults::QueryResults() : region_(nullptr), offset_(0u), bound_(false), ends_(0u)
{
    AV_SCOPE("QueryResults::QueryResults");
}

void QueryResults::reserve(ClientBlobArena &arena, uint8_t *view, uint64_t bytes)
{
    AV_SCOPE("QueryResults::reserve");
    constexpr uint64_t reserved = uint64_t{slots} * slot_bytes;
    if (view == nullptr || bytes <= reserved * 2u) return;
    offset_ = bytes - reserved;
    region_ = view + offset_;
    for (uint64_t i = 0u; i < reserved; i++) region_[i] = 0u;
    arena.lend(view, static_cast<size_t>(offset_));
}

void QueryResults::bind(ClientTransport &wire)
{
    AV_SCOPE("QueryResults::bind");
    if (bound_ || region_ == nullptr) return;
    bound_ = d3dproto::rvk_d3d_BindQueryResults(wire, offset_, slots);
}

uint64_t QueryResults::ended()
{
    AV_SCOPE("QueryResults::ended");
    return ++ends_;
}

bool QueryResults::read(uint32_t id, uint64_t end, void *data, uint32_t size) const
{
    AV_SCOPE("QueryResults::read");
    if (!bound_ || end == 0u || size > slot_bytes - data_at) return false;
    const uint8_t *const slot = region_ + static_cast<uint64_t>(id % slots) * slot_bytes;
    if (__atomic_load_n(reinterpret_cast<const uint64_t *>(slot), __ATOMIC_ACQUIRE) != end) return false;
    if (data != nullptr && size != 0u) __builtin_memcpy(data, slot + data_at, size);
    return true;
}

}  // namespace av::d3d11
