#include "core/memory/gpu_mmu_geometry.hpp"

#include "core/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "core/ddi/dxgk_page_table_level_desc_record.hpp"
#include "core/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

GpuMmuGeometry::GpuMmuGeometry(Platform &platform) : platform_(platform), virtual_address_bits_(48u), index_bits_(9u)
{
    AV_SCOPE("GpuMmuGeometry::GpuMmuGeometry");
}

Status GpuMmuGeometry::report_caps(const AdapterInfoQuery &query) const
{
    AV_SCOPE("GpuMmuGeometry::report_caps");
    platform_.require_irql_at_most(Irql::passive, "GpuMmuGeometry::report_caps");
    uint32_t physical_adapter = 0u;
    Status   status = query.read_input(&physical_adapter, physical_adapter_index_bytes);
    if (status == Status::ok && physical_adapter != 0u) status = Status::invalid_parameter;
    if (status != Status::ok) return status;
    DxgkGpuMmuCapsRecord caps = {};
    caps.Flags = cache_coherent_memory_supported;
    caps.PageTableUpdateMode = static_cast<int32_t>(update_through_cpu);
    caps.VirtualAddressBitCount = virtual_address_bits_;
    caps.PageTableLevelCount = level_count;
    return query.write_output(&caps, sizeof caps);
}

Status GpuMmuGeometry::report_level(const AdapterInfoQuery &query) const
{
    AV_SCOPE("GpuMmuGeometry::report_level");
    platform_.require_irql_at_most(Irql::passive, "GpuMmuGeometry::report_level");
    DxgkQueryPageTableLevelDescInRecord input = {};
    Status status = query.read_input(&input, sizeof input);
    if (status == Status::ok && (input.LevelIndex >= level_count || input.PhysicalAdapterIndex != 0u))
        status = Status::invalid_parameter;
    if (status != Status::ok) return status;
    const DxgkPageTableLevelDescRecord level = {index_bits_, system_memory_segment, system_memory_segment,
                                                entries_per_table * entry_bytes, 0u};
    return query.write_output(&level, sizeof level);
}

uint64_t GpuMmuGeometry::root_table_bytes(uint32_t entries) const
{
    AV_SCOPE("GpuMmuGeometry::root_table_bytes");
    platform_.require_irql_at_most(Irql::passive, "GpuMmuGeometry::root_table_bytes");
    return uint64_t{entries} * entry_bytes;
}

bool GpuMmuGeometry::root_reaches(uint64_t address, uint32_t root_entries) const
{
    AV_SCOPE("GpuMmuGeometry::root_reaches");
    platform_.require_irql_at_most(Irql::dispatch, "GpuMmuGeometry::root_reaches");
    const uint32_t root_shift = page_shift + index_bits_ * (level_count - 1u);
    return (address >> virtual_address_bits_) == 0u && (address >> root_shift) < root_entries;
}

}  // namespace av
