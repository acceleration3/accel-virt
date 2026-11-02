#include "miniport/memory/page_table_update.hpp"

#include "miniport/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "miniport/memory/gpu_mmu_geometry.hpp"
#include "miniport/memory/page_table_entry.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PageTableUpdate::PageTableUpdate(Platform &platform, const DxgkBuildPagingBufferUpdatePageTableRecord &update)
    : platform_(platform), update_(update)
    {
        AV_SCOPE("PageTableUpdate::PageTableUpdate");
    }

Status PageTableUpdate::write() const
{
    AV_SCOPE("PageTableUpdate::write");
    platform_.require_irql_at_most(Irql::passive, "PageTableUpdate::write");
    const DxgkBuildPagingBufferUpdatePageTableRecord &u = update_;
    const uint64_t end = uint64_t{u.StartIndex} + u.NumPageTableEntries;
    if (u.PageTableAddress == nullptr || u.pPageTableEntries == nullptr || u.UpdateMode != update_through_cpu ||
        u.PageTableLevel >= GpuMmuGeometry::level_count || end > GpuMmuGeometry::entries_per_table)
        return Status::invalid_parameter;
    const bool repeat = (u.Flags & flag_repeat) != 0u;
    WireWriter writer(static_cast<uint8_t *>(u.PageTableAddress) + u.StartIndex * GpuMmuGeometry::entry_bytes,
                      u.NumPageTableEntries * GpuMmuGeometry::entry_bytes);
    for (uint32_t index = 0u; index < u.NumPageTableEntries; index++)
        writer.write_u64(PageTableEntry(u.pPageTableEntries[repeat ? 0u : index]).encoded());
    return Status::ok;
}

}
