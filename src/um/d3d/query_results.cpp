#include "um/d3d/query_results.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/submission/published_slot_layout.hpp"

namespace av::d3d11 {

QueryResults::QueryResults() : reader_(), offset_(0u), bound_(false), ends_(0u)
{
    AV_SCOPE("QueryResults::QueryResults");
}

void QueryResults::reserve(ClientBlobArena &arena, uint8_t *view, uint64_t bytes)
{
    AV_SCOPE("QueryResults::reserve");
    constexpr uint64_t reserved = uint64_t{slots} * PublishedSlotLayout::slot_bytes;
    if (view == nullptr || bytes <= reserved * 2u) return;
    offset_ = PublishedSlotLayout(bytes).slots_at(slots);
    if (offset_ == 0u) return;
    reader_.take(view + offset_, slots);
    arena.lend(view, static_cast<size_t>(offset_));
}

void QueryResults::bind(ClientTransport &wire)
{
    AV_SCOPE("QueryResults::bind");
    if (bound_ || !reader_.reading()) return;
    bound_ = d3dproto::av_d3d_BindQueryResults(wire, offset_, slots);
}

uint64_t QueryResults::ended()
{
    AV_SCOPE("QueryResults::ended");
    return ++ends_;
}

bool QueryResults::read(uint32_t id, uint64_t end, void *data, uint32_t size) const
{
    AV_SCOPE("QueryResults::read");
    if (!bound_) return false;
    return reader_.read(id, end, data, size);
}

}  // namespace av::d3d11
