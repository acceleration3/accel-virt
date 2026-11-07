#include "miniport/memory/aperture_segment.hpp"

#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

ApertureSegment::ApertureSegment(Platform &platform) : platform_(platform), size_(0u), host_budget_(UINT64_MAX)
{
    AV_SCOPE("ApertureSegment::ApertureSegment");
}

Status ApertureSegment::initialize(int64_t system_memory_bytes)
{
    AV_SCOPE("ApertureSegment::initialize");
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::initialize");
    InvariantScope<ApertureSegment> guard(platform_, *this, "initialize");
    if (system_memory_bytes < static_cast<int64_t>(page_bytes)) return Status::invalid_parameter;
    size_ = static_cast<uint64_t>(system_memory_bytes) & ~(page_bytes - 1u);
    return Status::ok;
}

void ApertureSegment::offer(const CapsetInfo &capset)
{
    AV_SCOPE("ApertureSegment::offer");
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::offer");
    InvariantScope<ApertureSegment> guard(platform_, *this, "offer");
    const uint64_t budget = capset.gpu().shared_memory() & ~(page_bytes - 1u);
    host_budget_ = budget != 0u ? budget : UINT64_MAX;
}

void ApertureSegment::forget()
{
    AV_SCOPE("ApertureSegment::forget");
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::forget");
    host_budget_ = UINT64_MAX;
}

uint64_t ApertureSegment::commit_limit() const
{
    AV_SCOPE("ApertureSegment::commit_limit");
    return size_ < host_budget_ ? size_ : host_budget_;
}

void ApertureSegment::describe(DxgkSegmentDescriptor4Record &descriptor) const
{
    AV_SCOPE("ApertureSegment::describe");
    platform_.require_irql_at_most(Irql::passive, "ApertureSegment::describe");
    descriptor = DxgkSegmentDescriptor4Record{};
    descriptor.Flags = flags;
    descriptor.BaseAddress = base_address;
    descriptor.Size = size_;
    descriptor.CommitLimit = commit_limit();
}

uint64_t ApertureSegment::size() const
{
    AV_SCOPE("ApertureSegment::size");
    return size_;
}

bool ApertureSegment::invariants_hold() const
{
    AV_SCOPE("ApertureSegment::invariants_hold");
    return (size_ & (page_bytes - 1u)) == 0u && (host_budget_ == UINT64_MAX || (host_budget_ & (page_bytes - 1u)) == 0u);
}

}
