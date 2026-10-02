#include "miniport/memory/local_memory_segment.hpp"

#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

LocalMemorySegment::LocalMemorySegment(Platform &platform) : platform_(platform), size_(0u)
{
    AV_SCOPE("LocalMemorySegment::LocalMemorySegment");
}

void LocalMemorySegment::offer(const CapsetInfo &capset)
{
    AV_SCOPE("LocalMemorySegment::offer");
    platform_.require_irql_at_most(Irql::passive, "LocalMemorySegment::offer");
    InvariantScope<LocalMemorySegment> guard(platform_, *this, "offer");
    size_ = capset.gpu().local_memory() & ~(page_bytes - 1u);
}

void LocalMemorySegment::forget()
{
    AV_SCOPE("LocalMemorySegment::forget");
    platform_.require_irql_at_most(Irql::passive, "LocalMemorySegment::forget");
    size_ = 0u;
}

bool LocalMemorySegment::present() const
{
    AV_SCOPE("LocalMemorySegment::present");
    platform_.require_irql_at_most(Irql::dispatch, "LocalMemorySegment::present");
    return size_ != 0u;
}

/* No flag: not an aperture, not CPU-visible, and not preserved across standby,
 * since nothing in it would need preserving. */
void LocalMemorySegment::describe(DxgkSegmentDescriptor4Record &descriptor, int64_t base_address) const
{
    AV_SCOPE("LocalMemorySegment::describe");
    platform_.require_irql_at_most(Irql::passive, "LocalMemorySegment::describe");
    descriptor = DxgkSegmentDescriptor4Record{};
    descriptor.BaseAddress = base_address;
    descriptor.Size = size_;
    descriptor.CommitLimit = size_;
}

uint64_t LocalMemorySegment::size() const
{
    AV_SCOPE("LocalMemorySegment::size");
    return size_;
}

bool LocalMemorySegment::invariants_hold() const
{
    AV_SCOPE("LocalMemorySegment::invariants_hold");
    return (size_ & (page_bytes - 1u)) == 0u;
}

}  // namespace av
