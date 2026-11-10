#include "miniport/memory/memory_segments.hpp"

#include "miniport/ddi/dxgk_query_segment_out4_record.hpp"
#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/driver/adapter_info_query.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

MemorySegments::MemorySegments(Platform &platform, const ApertureSegment &aperture)
    : platform_(platform), aperture_(aperture), local_(platform)
    {
        AV_SCOPE("MemorySegments::MemorySegments");
    }

void MemorySegments::offer_local(const CapsetInfo &capset)
{
    AV_SCOPE("MemorySegments::offer_local");
    platform_.require_irql_at_most(Irql::passive, "MemorySegments::offer_local");
    local_.offer(capset);
}

void MemorySegments::forget_local()
{
    AV_SCOPE("MemorySegments::forget_local");
    platform_.require_irql_at_most(Irql::passive, "MemorySegments::forget_local");
    local_.forget();
}

uint32_t MemorySegments::count() const
{
    AV_SCOPE("MemorySegments::count");
    platform_.require_irql_at_most(Irql::dispatch, "MemorySegments::count");
    return local_.present() ? LocalMemorySegment::segment_id : ApertureSegment::segment_id;
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
    Status                     status = aperture_.size() == 0u ? Status::invalid_parameter : Status::ok;
    if (status == Status::ok) status = query.read_input(&physical_adapter, physical_adapter_bytes);
    if (status == Status::ok) status = query.read_output(&out, sizeof out);
    if (status != Status::ok || physical_adapter != 0u) return Status::invalid_parameter;
    if (out.pSegmentDescriptor != nullptr) {
        if (out.NbSegment < count() || out.SegmentDescriptorStride < sizeof(DxgkSegmentDescriptor4Record))
            return Status::invalid_parameter;
        describe(out);
    }
    out.NbSegment = count();
    return query.write_output(&out, sizeof out);
}

void MemorySegments::describe(DxgkQuerySegmentOut4Record &out) const
{
    AV_SCOPE("MemorySegments::describe");
    DxgkSegmentDescriptor4Record descriptor = {};
    aperture_.describe(descriptor);
    platform_.copy(out.pSegmentDescriptor, &descriptor, sizeof descriptor);
    if (local_.present()) {
        local_.describe(descriptor, ApertureSegment::base_address + static_cast<int64_t>(aperture_.size()));
        platform_.copy(out.pSegmentDescriptor + (LocalMemorySegment::segment_id - 1u) * out.SegmentDescriptorStride,
                       &descriptor, sizeof descriptor);
    }
    out.PagingBufferSegmentId = ApertureSegment::segment_id;
    out.PagingBufferSize = static_cast<uint32_t>(ApertureSegment::page_bytes);
    out.PagingBufferPrivateDataSize = 0u;
}

uint32_t MemorySegments::host_heap_segment_id(bool cpu_visible) const
{
    AV_SCOPE("MemorySegments::host_heap_segment_id");
    platform_.require_irql_at_most(Irql::dispatch, "MemorySegments::host_heap_segment_id");
    return !cpu_visible && local_.present() ? LocalMemorySegment::segment_id : system_memory_segment_id();
}

bool MemorySegments::invariants_hold() const
{
    AV_SCOPE("MemorySegments::invariants_hold");
    return aperture_.invariants_hold() && local_.invariants_hold();
}

}
