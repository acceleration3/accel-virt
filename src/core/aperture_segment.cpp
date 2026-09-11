#include "aperture_segment.hpp"

#include "adapter_info_query.hpp"
#include "ddi/dxgk_query_segment_out4_record.hpp"
#include "ddi/dxgk_segment_descriptor4_record.hpp"
#include "invariant_scope.hpp"

namespace av {

ApertureSegment::ApertureSegment(Platform &platform) : platform_(platform), size_(0u) {}

Status ApertureSegment::initialize(int64_t system_memory_bytes)
{
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::initialize");
    InvariantScope<ApertureSegment> guard(platform_, *this, "initialize");
    if (system_memory_bytes < static_cast<int64_t>(page_bytes)) return Status::invalid_parameter;
    size_ = static_cast<uint64_t>(system_memory_bytes) & ~(page_bytes - 1u);
    return Status::ok;
}

Status ApertureSegment::report(const AdapterInfoQuery &query) const
{
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::report");
    uint32_t physical_adapter = 0u;
    DxgkQuerySegmentOut4Record out = {};
    Status status = size_ == 0u ? Status::invalid_parameter : Status::ok;
    if (status == Status::ok) status = query.read_input(&physical_adapter, physical_adapter_bytes);
    if (status == Status::ok) status = query.read_output(&out, sizeof out);
    if (status != Status::ok || physical_adapter != 0u) return Status::invalid_parameter;
    if (out.pSegmentDescriptor != nullptr) {
        if (out.NbSegment < 1u || out.SegmentDescriptorStride < sizeof(DxgkSegmentDescriptor4Record))
            return Status::invalid_parameter;
        DxgkSegmentDescriptor4Record descriptor = {};
        descriptor.Flags = flags;
        descriptor.BaseAddress = base_address;
        descriptor.Size = size_;
        descriptor.CommitLimit = size_;
        platform_.copy(out.pSegmentDescriptor, &descriptor, sizeof descriptor);
        out.PagingBufferSegmentId = 0u;
        out.PagingBufferSize = static_cast<uint32_t>(page_bytes);
        out.PagingBufferPrivateDataSize = 0u;
    }
    out.NbSegment = 1u;
    return query.write_output(&out, sizeof out);
}

uint64_t ApertureSegment::size() const { return size_; }

bool ApertureSegment::invariants_hold() const { return (size_ & (page_bytes - 1u)) == 0u; }

}  // namespace av
