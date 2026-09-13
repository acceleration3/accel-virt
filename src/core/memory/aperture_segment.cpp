#include "core/memory/aperture_segment.hpp"

#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/support/invariant_scope.hpp"

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

Status ApertureSegment::describe(DxgkSegmentDescriptor4Record &descriptor) const
{
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::describe");
    if (size_ == 0u) return Status::invalid_state;
    descriptor = DxgkSegmentDescriptor4Record{};
    descriptor.Flags = flags;
    descriptor.BaseAddress = base_address;
    descriptor.Size = size_;
    descriptor.CommitLimit = size_;
    return Status::ok;
}

uint64_t ApertureSegment::size() const { return size_; }

bool ApertureSegment::invariants_hold() const { return (size_ & (page_bytes - 1u)) == 0u; }

}  // namespace av
