#include "miniport/ddi/dxgk_driver_caps_record.hpp"
#include "miniport/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "miniport/ddi/dxgk_node_metadata_record.hpp"
#include "miniport/ddi/dxgk_page_table_level_desc_record.hpp"
#include "miniport/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "miniport/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/vidmm/fake_gpu_mmu.hpp"
#include "harness/vidmm/fake_vidmm.hpp"
#include "harness/vidmm/gpu_mmu_rule.hpp"

#include <cstdint>
#include <cstring>

using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::DxgkGpuMmuCapsRecord;
using av::DxgkNodeMetadataRecord;
using av::DxgkPageTableLevelDescRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::FakeDxgadapter;
using av::FakeGpuMmu;
using av::GpuMmuRule;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::int32_t invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
constexpr std::int32_t configuration_error = static_cast<std::int32_t>(0xC0000182u);
constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

class GpuMmuMiniport {
public:
    GpuMmuMiniport() : caps_{}, physical_{}, mmu_{}, levels_{}, metadata_{}, fail_type_(0), fail_node_(false)
    {
        caps_.WDDMVersion = 0x2000;
        caps_.SchedulingCaps = 0x1u;
        caps_.MemoryManagementCaps = 0x60u;
        physical_.NumExecutionNodes = 1u;
        physical_.Flags = 0x2u;
        mmu_.VirtualAddressBitCount = 48u;
        mmu_.PageTableLevelCount = 4u;
        for (DxgkPageTableLevelDescRecord &level : levels_) level = {9u, 0u, 0u, 4096u, 0u};
        metadata_.GpuMmuSupported = 1u;
        set(DdiSlot::DxgkDdiQueryAdapterInfo, reinterpret_cast<DdiRoutine>(&GpuMmuMiniport::answer));
        set(DdiSlot::DxgkDdiGetNodeMetadata, reinterpret_cast<DdiRoutine>(&GpuMmuMiniport::metadata));
    }

    void set(DdiSlot slot, DdiRoutine routine)
    {
        if (table_.set(slot, routine) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "set failed");
    }
    av::DxgkDriverCapsRecord          &caps() { return caps_; }
    av::DxgkPhysicalAdapterCapsRecord &physical() { return physical_; }
    DxgkGpuMmuCapsRecord              &mmu() { return mmu_; }
    DxgkPageTableLevelDescRecord      &level(std::uint32_t index) { return levels_[index]; }
    DxgkNodeMetadataRecord            &node() { return metadata_; }
    void fail(std::int32_t type) { fail_type_ = type; }
    void fail_node() { fail_node_ = true; }
    const DdiTable &table() const { return table_; }

    static std::int32_t answer(void *context, const DxgkargQueryAdapterInfoRecord *query)
    { return static_cast<GpuMmuMiniport *>(context)->respond(*query); }
    static std::int32_t metadata(void *context, std::uint32_t, DxgkNodeMetadataRecord *out)
    {
        auto *self = static_cast<GpuMmuMiniport *>(context);
        *out = self->metadata_;
        return self->fail_node_ ? no_memory : 0;
    }

private:
    std::int32_t respond(const DxgkargQueryAdapterInfoRecord &query)
    {
        if (query.Type == fail_type_) return invalid_parameter;
        if (query.Type == 1) std::memcpy(query.pOutputData, &caps_, sizeof caps_);
        if (query.Type == 15) std::memcpy(query.pOutputData, &physical_, sizeof physical_);
        if (query.Type == 13) std::memcpy(query.pOutputData, &mmu_, sizeof mmu_);
        if (query.Type == 14) {
            av::DxgkQueryPageTableLevelDescInRecord in = {};
            std::memcpy(&in, query.pInputData, sizeof in);
            std::memcpy(query.pOutputData, &levels_[in.LevelIndex], sizeof levels_[0]);
        }
        return 0;
    }

    DdiTable                          table_;
    av::DxgkDriverCapsRecord          caps_;
    av::DxgkPhysicalAdapterCapsRecord physical_;
    DxgkGpuMmuCapsRecord              mmu_;
    DxgkPageTableLevelDescRecord      levels_[6];
    DxgkNodeMetadataRecord            metadata_;
    std::int32_t                      fail_type_;
    bool                              fail_node_;
};

std::int32_t start(GpuMmuMiniport &miniport, FakeDxgadapter &adapter, FakeGpuMmu &mmu)
{
    CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
    return mmu.query(&miniport);
}

}

TEST(fake_gpu_mmu_query, accepts_a_four_level_48_bit_geometry_and_vidmm_its_segments)
{ MockPlatform platform; GpuMmuMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeGpuMmu mmu(adapter);
  CHECK_EQ(start(miniport, adapter, mmu), 0);
  CHECK(mmu.supported());
  CHECK_EQ(mmu.caps().VirtualAddressBitCount, 48u);
  CHECK_EQ(mmu.level(3u).PageTableSizeInBytes, 4096u);
  CHECK(&mmu.level(9u) == &mmu.level(0u));
  CHECK_EQ(mmu.initialize_va_state(2u), 0);
  CHECK(mmu.failed_rule() == GpuMmuRule::none); }

TEST(fake_gpu_mmu_query, an_adapter_without_gpu_mmu_is_not_asked)
{ MockPlatform platform; GpuMmuMiniport miniport;
  miniport.physical().Flags = 0u;
  miniport.fail(13);
  FakeDxgadapter adapter(platform, miniport.table());
  FakeGpuMmu mmu(adapter);
  CHECK_EQ(start(miniport, adapter, mmu), 0);
  CHECK(!mmu.supported());
  CHECK_EQ(mmu.initialize_va_state(1u), 0); }

TEST(fake_gpu_mmu_query, refuses_each_description_dxgkrnl_refuses)
{ using Mutate = void (*)(GpuMmuMiniport &);
  struct Case { Mutate mutate; std::int32_t status; GpuMmuRule rule; };
  const Case cases[] = {
      {[](GpuMmuMiniport &m) { m.fail(13); }, configuration_error, GpuMmuRule::caps_query_failed},
      {[](GpuMmuMiniport &m) { m.mmu().VirtualAddressBitCount = 12u; }, invalid_parameter, GpuMmuRule::virtual_address_bits},
      {[](GpuMmuMiniport &m) { m.mmu().VirtualAddressBitCount = 64u; }, invalid_parameter, GpuMmuRule::virtual_address_bits},
      {[](GpuMmuMiniport &m) { m.mmu().PageTableLevelCount = 1u; }, configuration_error, GpuMmuRule::level_count},
      {[](GpuMmuMiniport &m) { m.mmu().PageTableLevelCount = 7u; }, configuration_error, GpuMmuRule::level_count},
      {[](GpuMmuMiniport &m) { m.fail(14); }, configuration_error, GpuMmuRule::level_query_failed},
      {[](GpuMmuMiniport &m) { m.level(2u).PageTableAlignmentInBytes = 3u; }, invalid_parameter, GpuMmuRule::level_alignment},
      {[](GpuMmuMiniport &m) { m.fail_node(); }, no_memory, GpuMmuRule::node_metadata_failed},
      {[](GpuMmuMiniport &m) { m.caps().MemoryManagementCaps = 0x20u; }, invalid_parameter, GpuMmuRule::node_gpu_mmu_without_caps},
      {[](GpuMmuMiniport &m) { m.node().Flags = 0x20u; }, invalid_parameter, GpuMmuRule::node_reserved_flags},
  };
  for (const Case &c : cases) {
      MockPlatform platform; GpuMmuMiniport miniport;
      c.mutate(miniport);
      FakeDxgadapter adapter(platform, miniport.table());
      FakeGpuMmu mmu(adapter);
      CHECK_EQ(start(miniport, adapter, mmu), c.status);
      CHECK(mmu.failed_rule() == c.rule);
  }
  MockPlatform platform; GpuMmuMiniport miniport;
  miniport.node().GpuMmuSupported = 0u;
  miniport.node().Flags = 0x1Fu;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeGpuMmu mmu(adapter);
  CHECK_EQ(start(miniport, adapter, mmu), 0); }

TEST(fake_gpu_mmu_initialize_va_state, refuses_each_description_vidmm_refuses)
{ using Mutate = void (*)(GpuMmuMiniport &);
  struct Case { Mutate mutate; std::uint32_t segments; GpuMmuRule rule; };
  const Case cases[] = {
      {[](GpuMmuMiniport &m) { m.level(0u).PageTableSizeInBytes = 0u; }, 2u, GpuMmuRule::leaf_table_size},
      {[](GpuMmuMiniport &m) { m.level(1u).PageTableSegmentId = 3u; }, 2u, GpuMmuRule::page_table_segment},
      {[](GpuMmuMiniport &m) { m.level(0u).PagingProcessPageTableSegmentId = 3u; }, 2u, GpuMmuRule::paging_process_page_table_segment},
      {[](GpuMmuMiniport &m) { m.level(0u).PageTableIndexBitCount = 0u; }, 2u, GpuMmuRule::leaf_index_bits},
      {[](GpuMmuMiniport &m) { m.level(0u).PageTableIndexBitCount = 36u; }, 2u, GpuMmuRule::leaf_index_bits},
      {[](GpuMmuMiniport &m) { m.mmu().PageTableUpdateMode = 3; }, 2u, GpuMmuRule::update_mode},
      {[](GpuMmuMiniport &m) { m.level(2u).PageTableIndexBitCount = 0u; }, 2u, GpuMmuRule::directory_index_bits},
      {[](GpuMmuMiniport &m) { m.level(3u).PageTableIndexBitCount = 10u; }, 2u, GpuMmuRule::directory_index_bits},
      {[](GpuMmuMiniport &m) { m.level(1u).PageTableSizeInBytes = 0u; }, 2u, GpuMmuRule::directory_table_size},
      {[](GpuMmuMiniport &m) { m.level(3u).PageTableIndexBitCount = 8u; }, 2u, GpuMmuRule::total_index_bits},
  };
  for (const Case &c : cases) {
      MockPlatform platform; GpuMmuMiniport miniport;
      c.mutate(miniport);
      FakeDxgadapter adapter(platform, miniport.table());
      FakeGpuMmu mmu(adapter);
      CHECK_EQ(start(miniport, adapter, mmu), 0);
      CHECK_EQ(mmu.initialize_va_state(c.segments), no_memory);
      CHECK(mmu.failed_rule() == c.rule);
  }
  MockPlatform platform; GpuMmuMiniport miniport;
  miniport.mmu().PageTableLevelCount = 2u;
  miniport.mmu().VirtualAddressBitCount = 32u;
  miniport.level(1u).PageTableSizeInBytes = 0u;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeGpuMmu mmu(adapter);
  CHECK_EQ(start(miniport, adapter, mmu), 0);
  CHECK_EQ(mmu.initialize_va_state(2u), 0); }

TEST(fake_vidmm_segment_count, adds_the_system_memory_segment_for_virtual_addressing)
{ MockPlatform platform; GpuMmuMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  av::FakeVidMm vidmm(adapter);
  CHECK_EQ(vidmm.segment_count(), 1u);
  miniport.physical().Flags = 0u;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(vidmm.segment_count(), 0u); }
