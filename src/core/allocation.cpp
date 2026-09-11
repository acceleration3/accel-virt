#include "allocation.hpp"

#include "aperture_segment.hpp"
#include "ddi/dxgk_allocation_info_record.hpp"
#include "invariant_scope.hpp"

namespace av {

Allocation::Allocation(Platform &platform, const AllocationDescriptor &descriptor, uint64_t size)
    : platform_(platform), descriptor_(descriptor), size_(size), opens_(0u) {}

const AllocationDescriptor &Allocation::descriptor() const { return descriptor_; }
uint64_t                    Allocation::size() const { return size_; }
uint32_t                    Allocation::opens() const { return opens_; }

void Allocation::report_to(DxgkAllocationInfoRecord &info)
{
    platform_.require_irql_at_most(Irql::apc, "Allocation::report_to");
    info.Size = size_;
    info.Alignment = 0u;
    info.PitchAlignedSize = 0u;
    info.HintedBank = 0u;
    info.PreferredSegment = ApertureSegment::segment_id;
    info.SupportedReadSegmentSet = 1u << (ApertureSegment::segment_id - 1u);
    info.SupportedWriteSegmentSet = 1u << (ApertureSegment::segment_id - 1u);
    info.EvictionSegmentSet = 0u;
    info.MaximumRenamingListLength = 0u;
    info.hAllocation = this;
    info.Flags = descriptor_.cpu_visible() ? flag_cpu_visible : 0u;
    info.AllocationPriority = priority_normal;
    info.Flags2 = 0u;
}

Status Allocation::open()
{
    platform_.require_irql_at_most(Irql::apc, "Allocation::open");
    InvariantScope<Allocation> guard(platform_, *this, "open");
    opens_++;
    return Status::ok;
}

Status Allocation::close()
{
    platform_.require_irql_at_most(Irql::apc, "Allocation::close");
    InvariantScope<Allocation> guard(platform_, *this, "close");
    if (opens_ == 0u) return Status::invalid_state;
    opens_--;
    return Status::ok;
}

bool Allocation::invariants_hold() const
{
    return size_ != 0u && (size_ & (page_bytes - 1u)) == 0u && size_ >= descriptor_.size();
}

}  // namespace av
