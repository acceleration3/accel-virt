#include "core/memory/memory_segments.hpp"

#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/miniport/adapter_info_query.hpp"

namespace av {

MemorySegments::MemorySegments(Platform &platform, const ApertureSegment &aperture,
                               const HostVisibleSegment &host_visible)
    : platform_(platform), aperture_(aperture), host_visible_(host_visible) {}

uint32_t MemorySegments::count() const
{
    platform_.require_irql_at_most(Irql::dispatch, "MemorySegments::count");
    return host_visible_.offered() ? 2u : 1u;
}

uint32_t MemorySegments::system_memory_segment_id() const
{
    platform_.require_irql_at_most(Irql::dispatch, "MemorySegments::system_memory_segment_id");
    return count() + 1u;
}

Status MemorySegments::report(const AdapterInfoQuery &query) const
{
    platform_.require_irql_at_most(Irql::passive, "MemorySegments::report");
    uint32_t                   physical_adapter = 0u;
    DxgkQuerySegmentOut4Record out = {};
    /* Before StartDevice there is no memory to describe, and VidMm asking then
     * is asking about an adapter that has not been built. */
    Status                     status = aperture_.size() == 0u ? Status::invalid_parameter : Status::ok;
    if (status == Status::ok) status = query.read_input(&physical_adapter, physical_adapter_bytes);
    if (status == Status::ok) status = query.read_output(&out, sizeof out);
    if (status != Status::ok || physical_adapter != 0u) return Status::invalid_parameter;
    const uint32_t segments = count();
    if (out.pSegmentDescriptor != nullptr) {
        if (out.NbSegment < segments || out.SegmentDescriptorStride < sizeof(DxgkSegmentDescriptor4Record))
            return Status::invalid_parameter;
        DxgkSegmentDescriptor4Record descriptor = {};
        status = aperture_.describe(descriptor);
        if (status != Status::ok) return status;
        auto *bytes = static_cast<uint8_t *>(out.pSegmentDescriptor);
        platform_.copy(bytes, &descriptor, sizeof descriptor);
        if (segments > 1u) {
            status = host_visible_.describe(descriptor);
            if (status != Status::ok) return status;
            platform_.copy(bytes + out.SegmentDescriptorStride, &descriptor, sizeof descriptor);
        }
        out.PagingBufferSegmentId = ApertureSegment::segment_id;
        out.PagingBufferSize = static_cast<uint32_t>(ApertureSegment::page_bytes);
        out.PagingBufferPrivateDataSize = 0u;
    }
    out.NbSegment = segments;
    return query.write_output(&out, sizeof out);
}

bool MemorySegments::invariants_hold() const
{
    return aperture_.invariants_hold() && host_visible_.invariants_hold();
}

}  // namespace av
