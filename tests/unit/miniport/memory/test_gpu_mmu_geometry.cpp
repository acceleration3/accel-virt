#include "miniport/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "miniport/ddi/dxgk_page_table_level_desc_record.hpp"
#include "miniport/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/memory/gpu_mmu_geometry.hpp"
#include "miniport/driver/adapter_info_query.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterInfoQuery;
using av::DxgkGpuMmuCapsRecord;
using av::DxgkPageTableLevelDescRecord;
using av::DxgkQueryPageTableLevelDescInRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::GpuMmuGeometry;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

DxgkargQueryAdapterInfoRecord query_of(std::int32_t type, void *input, std::uint32_t input_size, void *output,
                                       std::uint32_t output_size)
{
    DxgkargQueryAdapterInfoRecord record = {};
    record.Type = type;
    record.pInputData = input;
    record.InputDataSize = input_size;
    record.pOutputData = output;
    record.OutputDataSize = output_size;
    return record;
}

}  // namespace

TEST(gpu_mmu_geometry_report_caps, describes_four_levels_over_48_bits_updated_through_the_cpu)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  std::uint32_t physical_adapter = 0u;
  DxgkGpuMmuCapsRecord caps;
  std::memset(&caps, 0xEE, sizeof caps);
  const DxgkargQueryAdapterInfoRecord record = query_of(13, &physical_adapter, 4u, &caps, sizeof caps);
  CHECK_STATUS(geometry.report_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.Flags, 0x10u);   /* CacheCoherentMemorySupported */
  CHECK_EQ(caps.PageTableUpdateMode, 0);
  CHECK_EQ(caps.VirtualAddressBitCount, 48u);
  CHECK_EQ(caps.LeafPageTableSizeFor64KPagesInBytes, 0u);
  CHECK_EQ(caps.PageTableLevelCount, 4u);
  CHECK_EQ(caps.LegacyBehaviors, 0u); }

TEST(gpu_mmu_geometry_report_caps, another_physical_adapter_or_short_buffers_are_invalid)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  std::uint32_t physical_adapter = 1u;
  DxgkGpuMmuCapsRecord caps = {};
  CHECK_STATUS(geometry.report_caps(AdapterInfoQuery(platform, query_of(13, &physical_adapter, 4u, &caps, sizeof caps))),
               Status::invalid_parameter);
  physical_adapter = 0u;
  CHECK_STATUS(geometry.report_caps(AdapterInfoQuery(platform, query_of(13, &physical_adapter, 3u, &caps, sizeof caps))),
               Status::invalid_parameter);
  CHECK_STATUS(geometry.report_caps(AdapterInfoQuery(platform, query_of(13, &physical_adapter, 4u, &caps, 0x17u))),
               Status::invalid_parameter);
  CHECK_EQ(caps.VirtualAddressBitCount, 0u); }

TEST(gpu_mmu_geometry_report_level, every_level_is_a_4096_byte_table_of_512_entries_in_system_memory)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  for (std::uint16_t level = 0u; level < GpuMmuGeometry::level_count; level++) {
      DxgkQueryPageTableLevelDescInRecord input = {level, 0u};
      DxgkPageTableLevelDescRecord desc;
      std::memset(&desc, 0xEE, sizeof desc);
      CHECK_STATUS(geometry.report_level(AdapterInfoQuery(platform, query_of(14, &input, sizeof input, &desc, sizeof desc))),
                   Status::ok);
      CHECK_EQ(desc.PageTableIndexBitCount, 9u);
      CHECK_EQ(desc.PageTableSegmentId, 0u);
      CHECK_EQ(desc.PagingProcessPageTableSegmentId, 0u);
      CHECK_EQ(desc.PageTableSizeInBytes, 4096u);
      CHECK_EQ(desc.PageTableAlignmentInBytes, 0u);
  } }

TEST(gpu_mmu_geometry_report_level, a_level_past_the_root_or_another_adapter_is_invalid)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  const DxgkQueryPageTableLevelDescInRecord inputs[] = {{4u, 0u}, {0u, 1u}};
  for (DxgkQueryPageTableLevelDescInRecord input : inputs) {
      DxgkPageTableLevelDescRecord desc = {};
      CHECK_STATUS(geometry.report_level(AdapterInfoQuery(platform, query_of(14, &input, sizeof input, &desc, sizeof desc))),
                   Status::invalid_parameter);
      CHECK_EQ(desc.PageTableSizeInBytes, 0u);
  }
  DxgkPageTableLevelDescRecord desc = {};
  CHECK_STATUS(geometry.report_level(AdapterInfoQuery(platform, query_of(14, nullptr, 0u, &desc, sizeof desc))),
               Status::invalid_parameter); }

TEST(gpu_mmu_geometry_root_table_bytes, is_eight_bytes_an_entry)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  CHECK_EQ(geometry.root_table_bytes(512u), 4096u);
  CHECK_EQ(geometry.root_table_bytes(0xFFFFFFFFu), 0x7FFFFFFF8ull); }

TEST(gpu_mmu_geometry_root_reaches, each_root_entry_covers_512_gigabytes)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  CHECK(geometry.root_reaches(0u, 1u));
  CHECK(geometry.root_reaches(0x7FFFFFFFFFull, 1u));
  CHECK(!geometry.root_reaches(0x8000000000ull, 1u));
  CHECK(geometry.root_reaches(0xFFFFFFFFFFFFull, 512u));
  CHECK(!geometry.root_reaches(0x1000000000000ull, 0xFFFFFFFFu));
  CHECK(!geometry.root_reaches(0u, 0u)); }

TEST(gpu_mmu_geometry, is_passive_only)
{ MockPlatform platform; const GpuMmuGeometry geometry(platform);
  const DxgkargQueryAdapterInfoRecord record = query_of(13, nullptr, 0u, nullptr, 0u);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(geometry.report_caps(AdapterInfoQuery(platform, record)));
  static_cast<void>(geometry.report_level(AdapterInfoQuery(platform, record)));
  static_cast<void>(geometry.root_table_bytes(1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }
