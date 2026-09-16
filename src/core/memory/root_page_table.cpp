#include "core/memory/root_page_table.hpp"

#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/memory/gpu_mmu_geometry.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

RootPageTable::RootPageTable(Platform &platform, const GpuMmuGeometry &geometry)
    : platform_(platform), geometry_(geometry), segment_id_(0u), segment_offset_(0u), entries_(0u)
    {
        AV_SCOPE("RootPageTable::RootPageTable");
    }

Status RootPageTable::assign(const DxgkargSetRootPageTableRecord &root)
{
    AV_SCOPE("RootPageTable::assign");
    platform_.require_irql_at_most(Irql::passive, "RootPageTable::assign");
    InvariantScope<RootPageTable> guard(platform_, *this, "assign");
    if (root.NumEntries == 0u || root.NumEntries > GpuMmuGeometry::entries_per_table) return Status::invalid_parameter;
    segment_id_ = root.AddressSegmentId;
    segment_offset_ = root.AddressSegmentOffset;
    entries_ = root.NumEntries;
    return Status::ok;
}

bool RootPageTable::admits(uint64_t address) const
{
    AV_SCOPE("RootPageTable::admits");
    platform_.require_irql_at_most(Irql::dispatch, "RootPageTable::admits");
    return entries_ == 0u || geometry_.root_reaches(address, entries_);
}

bool RootPageTable::invariants_hold() const
{
    AV_SCOPE("RootPageTable::invariants_hold");
    return entries_ <= GpuMmuGeometry::entries_per_table;
}

}  // namespace av
