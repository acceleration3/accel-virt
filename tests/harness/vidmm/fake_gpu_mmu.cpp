#include "harness/vidmm/fake_gpu_mmu.hpp"

#include "core/ddi/dxgk_node_metadata_record.hpp"
#include "core/ddi/dxgk_query_page_table_level_desc_in_record.hpp"

namespace av {

namespace {

constexpr std::int32_t  query_gpu_mmu_caps = 13;          /* DXGKQAITYPE_GPUMMUCAPS */
constexpr std::int32_t  query_page_table_level_desc = 14; /* DXGKQAITYPE_PAGETABLELEVELDESC */
constexpr std::uint32_t lowest_virtual_address_bits = 13u;
constexpr std::uint32_t virtual_address_bit_span = 50u;
constexpr std::uint32_t lowest_level_count = 2u;

}  // namespace

FakeGpuMmu::FakeGpuMmu(FakeDxgadapter &adapter) : adapter_(adapter), caps_{}, levels_{}, failed_(GpuMmuRule::none) {}

std::int32_t FakeGpuMmu::refuse(GpuMmuRule rule, std::int32_t status)
{
    failed_ = rule;
    return status;
}

bool FakeGpuMmu::supported() const { return (adapter_.physical_adapter_caps().Flags & physical_gpu_mmu) != 0u; }

std::int32_t FakeGpuMmu::query(void *context)
{
    failed_ = GpuMmuRule::none;
    caps_ = DxgkGpuMmuCapsRecord{};
    if (!supported()) return 0;
    std::uint32_t      physical_adapter = 0u;
    const std::int32_t status = adapter_.query(context, query_gpu_mmu_caps, &physical_adapter, sizeof physical_adapter,
                                               &caps_, sizeof caps_);
    if (status < 0) return refuse(GpuMmuRule::caps_query_failed, status_device_configuration_error);
    if (caps_.VirtualAddressBitCount - lowest_virtual_address_bits > virtual_address_bit_span)
        return refuse(GpuMmuRule::virtual_address_bits, FakeDxgadapter::status_invalid_parameter);
    if (caps_.PageTableLevelCount < lowest_level_count || caps_.PageTableLevelCount > max_levels)
        return refuse(GpuMmuRule::level_count, status_device_configuration_error);
    const std::int32_t levels = query_levels(context);
    return levels < 0 ? levels : check_nodes(context);
}

std::int32_t FakeGpuMmu::query_levels(void *context)
{
    for (std::uint32_t level = 0u; level < caps_.PageTableLevelCount; level++) {
        DxgkQueryPageTableLevelDescInRecord input = {static_cast<std::uint16_t>(level), 0u};
        if (adapter_.query(context, query_page_table_level_desc, &input, sizeof input, &levels_[level],
                           sizeof levels_[level]) < 0)
            return refuse(GpuMmuRule::level_query_failed, status_device_configuration_error);
        const std::uint32_t alignment = levels_[level].PageTableAlignmentInBytes;
        if ((alignment & (alignment - 1u)) != 0u)
            return refuse(GpuMmuRule::level_alignment, FakeDxgadapter::status_invalid_parameter);
    }
    return 0;
}

std::int32_t FakeGpuMmu::check_nodes(void *context)
{
    const DxgkDriverCapsRecord &driver = adapter_.driver_caps();
    for (std::uint32_t node = 0u; node < adapter_.physical_adapter_caps().NumExecutionNodes; node++) {
        DxgkNodeMetadataRecord metadata = {};
        const std::int32_t     status = adapter_.node_metadata(context, node, metadata);
        if (status < 0) return refuse(GpuMmuRule::node_metadata_failed, status);
        if (metadata.GpuMmuSupported != 0u && (driver.MemoryManagementCaps & memory_gpu_mmu) == 0u)
            return refuse(GpuMmuRule::node_gpu_mmu_without_caps, FakeDxgadapter::status_invalid_parameter);
        if ((metadata.Flags & node_reserved_flags) != 0u)
            return refuse(GpuMmuRule::node_reserved_flags, FakeDxgadapter::status_invalid_parameter);
    }
    return 0;
}

/* Level 0 is the leaf. A two-level table skips the directory checks. */
std::int32_t FakeGpuMmu::initialize_va_state(std::uint32_t segments)
{
    failed_ = GpuMmuRule::none;
    if (!supported()) return 0;
    const std::uint32_t index_bits = caps_.VirtualAddressBitCount - page_shift;
    const DxgkPageTableLevelDescRecord &leaf = levels_[0];
    if (leaf.PageTableSizeInBytes == 0u) return refuse(GpuMmuRule::leaf_table_size, status_no_memory);
    for (std::uint32_t level = 0u; level < 2u; level++) {
        if (levels_[level].PageTableSegmentId > segments)
            return refuse(GpuMmuRule::page_table_segment, status_no_memory);
        if (levels_[level].PagingProcessPageTableSegmentId > segments)
            return refuse(GpuMmuRule::paging_process_page_table_segment, status_no_memory);
    }
    if (leaf.PageTableIndexBitCount == 0u || leaf.PageTableIndexBitCount >= index_bits)
        return refuse(GpuMmuRule::leaf_index_bits, status_no_memory);
    if (static_cast<std::uint32_t>(caps_.PageTableUpdateMode) > highest_update_mode)
        return refuse(GpuMmuRule::update_mode, status_no_memory);
    const GpuMmuRule rule = check_directories();
    return rule == GpuMmuRule::none ? 0 : refuse(rule, status_no_memory);
}

GpuMmuRule FakeGpuMmu::check_directories() const
{
    if (caps_.PageTableLevelCount == lowest_level_count) return GpuMmuRule::none;
    const std::uint32_t index_bits = caps_.VirtualAddressBitCount - page_shift;
    std::uint32_t       used = levels_[0].PageTableIndexBitCount;
    for (std::uint32_t level = 1u; level < caps_.PageTableLevelCount; level++) {
        const std::uint32_t bits = levels_[level].PageTableIndexBitCount;
        if (bits == 0u || bits > index_bits - used) return GpuMmuRule::directory_index_bits;
        if (levels_[level].PageTableSizeInBytes == 0u) return GpuMmuRule::directory_table_size;
        used += bits;
    }
    return used == index_bits ? GpuMmuRule::none : GpuMmuRule::total_index_bits;
}

GpuMmuRule                          FakeGpuMmu::failed_rule() const { return failed_; }
const DxgkGpuMmuCapsRecord         &FakeGpuMmu::caps() const { return caps_; }
const DxgkPageTableLevelDescRecord &FakeGpuMmu::level(std::uint32_t index) const
{
    return levels_[index < max_levels ? index : 0u];
}

}  // namespace av
