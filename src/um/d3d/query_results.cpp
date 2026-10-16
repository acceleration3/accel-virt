#include "um/d3d/query_results.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

namespace av::d3d11 {

QueryResults::QueryResults() : reader_(), offset_(0u), bound_(false), ends_(0u)
{
    AV_SCOPE("QueryResults::QueryResults");
}

void QueryResults::take(uint8_t *view, uint64_t offset)
{
    AV_SCOPE("QueryResults::take");
    offset_ = offset;
    reader_.take(view, slots);
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
