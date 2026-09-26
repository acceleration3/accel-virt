#include "core/memory/memory_segments.hpp"

#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

MemorySegments::MemorySegments(Platform &platform, const ApertureSegment &aperture)
    : platform_(platform), aperture_(aperture)
    {
        AV_SCOPE("MemorySegments::MemorySegments");
    }

uint32_t MemorySegments::count() const
{
    AV_SCOPE("MemorySegments::count");
    platform_.require_irql_at_most(Irql::dispatch, "MemorySegments::count");
    return segments;
}

uint32_t MemorySegments::system_memory_segment_id() const
{
    AV_SCOPE("MemorySegments::system_memory_segment_id");
    platform_.require_irql_at_most(Irql::dispatch, "MemorySegments::system_memory_segment_id");
    return count() + 1u;
}

Status MemorySegments::report(const AdapterInfoQuery &query) const
{
    AV_SCOPE("MemorySegments::report");
    platform_.require_irql_at_most(Irql::passive, "MemorySegments::report");
    uint32_t                   physical_adapter = 0u;
    DxgkQuerySegmentOut4Record out = {};
    /* Before StartDevice there is no memory to describe, and VidMm asking then
     * is asking about an adapter that has not been built. */
    Status                     status = aperture_.size() == 0u ? Status::invalid_parameter : Status::ok;
    if (status == Status::ok) status = query.read_input(&physical_adapter, physical_adapter_bytes);
    if (status == Status::ok) status = query.read_output(&out, sizeof out);
    if (status != Status::ok || physical_adapter != 0u) return Status::invalid_parameter;
    if (out.pSegmentDescriptor != nullptr) {
        if (out.NbSegment < segments || out.SegmentDescriptorStride < sizeof(DxgkSegmentDescriptor4Record))
            return Status::invalid_parameter;
        /* The aperture exists by now: a sized one is what the guard above
         * demands. */
        DxgkSegmentDescriptor4Record descriptor = {};
        aperture_.describe(descriptor);
        platform_.copy(out.pSegmentDescriptor, &descriptor, sizeof descriptor);
        out.PagingBufferSegmentId = ApertureSegment::segment_id;
        out.PagingBufferSize = static_cast<uint32_t>(ApertureSegment::page_bytes);
        out.PagingBufferPrivateDataSize = 0u;
    }
    out.NbSegment = segments;
    return query.write_output(&out, sizeof out);
}

bool MemorySegments::invariants_hold() const
{
    AV_SCOPE("MemorySegments::invariants_hold");
    return aperture_.invariants_hold();
}

}  // namespace av
