#include "miniport/ddi/dxgk_driver_caps_record.hpp"
#include "miniport/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/capability_rule.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::CapabilityRule;
using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::DxgkDriverCapsRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::FakeDxgadapter;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::int32_t invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
constexpr std::int32_t revision_mismatch = static_cast<std::int32_t>(0xC0000059u);
constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);
constexpr std::int32_t not_supported = static_cast<std::int32_t>(0xC00000BBu);
constexpr std::uint32_t max_type = 48u;

class CraftedMiniport {
public:
    CraftedMiniport() : caps_{}, nodes_(1u), save_size_(0u), statuses_{}
    {
        caps_.WDDMVersion = 0x1000;
        set(DdiSlot::DxgkDdiQueryAdapterInfo, reinterpret_cast<DdiRoutine>(&CraftedMiniport::answer));
    }

    void set(DdiSlot slot, DdiRoutine routine)
    {
        if (table_.set(slot, routine) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "set failed");
    }
    void register_slot(DdiSlot slot) { set(slot, reinterpret_cast<DdiRoutine>(&CraftedMiniport::answer)); }
    DxgkDriverCapsRecord &caps() { return caps_; }
    void fail_query(std::int32_t type, std::int32_t status) { statuses_[type] = status; }
    void set_nodes(std::uint16_t nodes) { nodes_ = nodes; }
    void set_save_size(std::uint64_t size) { save_size_ = size; }
    const DdiTable &table() const { return table_; }

    static std::int32_t answer(void *context, const DxgkargQueryAdapterInfoRecord *query)
    { return static_cast<CraftedMiniport *>(context)->respond(*query); }

private:
    std::int32_t respond(const DxgkargQueryAdapterInfoRecord &query)
    {
        if (statuses_[query.Type] != 0) return statuses_[query.Type];
        if (query.Type == 1) std::memcpy(query.pOutputData, &caps_, sizeof caps_);
        if (query.Type == 15) std::memcpy(query.pOutputData, &nodes_, sizeof nodes_);
        if (query.Type == 21) std::memcpy(query.pOutputData, &save_size_, sizeof save_size_);
        return 0;
    }

    DdiTable             table_;
    DxgkDriverCapsRecord caps_;
    std::uint16_t        nodes_;
    std::uint64_t        save_size_;
    std::int32_t         statuses_[max_type + 1u];
};

}

TEST(fake_dxgadapter_initialize, accepts_wddm_1_0_with_no_capability)
{ MockPlatform platform; CraftedMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK(adapter.failed_rule() == CapabilityRule::none);
  CHECK_EQ(adapter.driver_caps().WDDMVersion, 0x1000);
  CHECK_EQ(adapter.queries(), 2u);   }

TEST(fake_dxgadapter_initialize, a_failed_driver_caps_query_fails_start_with_its_status)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.fail_query(1, no_memory);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), no_memory);
  CHECK(adapter.failed_rule() == CapabilityRule::driver_caps_query_failed); }

TEST(fake_dxgadapter_initialize, no_query_adapter_info_is_a_revision_mismatch)
{ MockPlatform platform; DdiTable table;
  FakeDxgadapter adapter(platform, table);
  CHECK_EQ(adapter.initialize(nullptr, 1u), revision_mismatch);
  CHECK(adapter.failed_rule() == CapabilityRule::driver_caps_query_failed); }

namespace {
std::int32_t mode_status = 0;
std::uint32_t mode_satisfied = 0u;
std::uint32_t mode_not_satisfied = 0u;
std::uint32_t mode_requests = 0u;

std::int32_t control_mode_behavior(void *, av::DxgkargControlModeBehaviorRecord *request)
{
    mode_requests = request->Request;
    request->Satisfied = mode_satisfied;
    request->NotSatisfied = mode_not_satisfied;
    return mode_status;
}

}

TEST(fake_dxgadapter_initialize, an_adapter_with_outputs_is_asked_to_prioritize_hdr_first)
{ MockPlatform platform; CraftedMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.mode_behavior().Request, 0u);
  miniport.set(DdiSlot::DxgkDdiControlModeBehavior, reinterpret_cast<DdiRoutine>(&control_mode_behavior));
  mode_status = 0; mode_satisfied = 0u; mode_not_satisfied = 1u; mode_requests = 0u;
  CHECK_EQ(adapter.initialize(&miniport, 0u), 0);
  CHECK_EQ(mode_requests, 0u);
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(mode_requests, 1u);
  CHECK_EQ(adapter.mode_behavior().NotSatisfied, 1u);
  CHECK(!av::TestRegistry::instance().take_current_failure());
  mode_status = no_memory;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  mode_status = not_supported;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK(!av::TestRegistry::instance().take_current_failure());
  mode_status = invalid_parameter;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK(av::TestRegistry::instance().take_current_failure());
  mode_status = 0; mode_satisfied = 1u;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK(av::TestRegistry::instance().take_current_failure()); }

TEST(fake_dxgadapter_initialize, a_wddm_2_display_adapter_is_asked_for_its_caps_extension)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x2000;
  miniport.caps().SchedulingCaps = 0x1u;
  miniport.fail_query(16, invalid_parameter);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.display_caps_extension(), 0u);
  const std::uint32_t asked = adapter.queries();
  miniport.caps().WDDMVersion = 0x2200;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.display_caps_extension(), 0x2u);
  miniport.fail_query(16, 0);
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.display_caps_extension(), 0u);
  CHECK_EQ(adapter.initialize(&miniport, 0u), 0);
  CHECK_EQ(adapter.queries(), asked - 1u);
  miniport.caps().WDDMVersion = 0x1000;
  miniport.caps().SchedulingCaps = 0u;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.queries(), asked - 2u); }

TEST(fake_dxgadapter_initialize, a_wddm_3_2_display_adapter_needs_monitor_link_info)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x3200;
  miniport.caps().SchedulingCaps = 0x1u;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), revision_mismatch);
  CHECK(adapter.failed_rule() == CapabilityRule::wddm_3_2_display_without_monitor_link_info);
  CHECK_EQ(adapter.initialize(&miniport, 0u), 0);
  miniport.fail_query(1, no_memory);
  CHECK_EQ(adapter.initialize(&miniport, 1u), no_memory);
  miniport.fail_query(1, 0);
  miniport.register_slot(DdiSlot::DxgkDdiUpdateMonitorLinkInfo);
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  miniport.caps().WDDMVersion = 0x3100;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0); }

TEST(fake_dxgadapter_query, a_status_dxgkrnl_logs_as_invalid_fails_the_test)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.fail_query(13, not_supported);
  FakeDxgadapter adapter(platform, miniport.table());
  std::uint8_t output[8] = {};
  CHECK_EQ(adapter.query(&miniport, 13, nullptr, 0u, output, sizeof output), not_supported);
  CHECK(av::TestRegistry::instance().take_current_failure());
  miniport.fail_query(13, invalid_parameter);
  CHECK_EQ(adapter.query(&miniport, 13, nullptr, 0u, output, sizeof output), invalid_parameter);
  CHECK(!av::TestRegistry::instance().take_current_failure()); }

TEST(fake_dxgadapter_validate, refuses_each_claim_dxgadapter_initialize_refuses)
{ using Mutate = void (*)(DxgkDriverCapsRecord &);
  struct Case { Mutate mutate; CapabilityRule rule; };
  const Case cases[] = {
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2400; c.MiscCaps = 0x200u; }, CapabilityRule::misc_caps_reserved},
      {[](DxgkDriverCapsRecord &c) { c.MiscCaps = 0x10u; }, CapabilityRule::vsync_control_without_control_interrupt3},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2105; }, CapabilityRule::unknown_wddm_version},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0; }, CapabilityRule::unknown_wddm_version},
      {[](DxgkDriverCapsRecord &c) { c.SupportDirectFlip = 1u; }, CapabilityRule::wddm_1_2_feature_below_1_2},
      {[](DxgkDriverCapsRecord &c) { c.SchedulingCaps = 0x4u; }, CapabilityRule::wddm_1_2_feature_below_1_2},
      {[](DxgkDriverCapsRecord &c) { c.PreemptionCapsComputePreemptionGranularity = 100; }, CapabilityRule::wddm_1_2_feature_below_1_2},
      {[](DxgkDriverCapsRecord &c) { c.PresentationCaps = 0x10000000u; }, CapabilityRule::wddm_1_2_feature_below_1_2},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1200; c.FlipCaps = 0x10u; }, CapabilityRule::wddm_1_3_feature_below_1_3},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1200; c.MaxOverlayPlanes = 1u; }, CapabilityRule::wddm_1_3_feature_below_1_3},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1300; c.HybridIntegrated = 1u; }, CapabilityRule::hybrid_integrated_below_2_0},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2900; c.MemoryManagementCaps = 0x8000u; }, CapabilityRule::cross_adapter_tiers},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2900; c.MemoryManagementCaps = 0x10010u; }, CapabilityRule::cross_adapter_tiers},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2000; c.HybridDiscrete = 1u; }, CapabilityRule::hybrid_without_cross_adapter_resource},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2000; c.MemoryManagementCaps = 0x10u; c.HybridDiscrete = 1u; c.HybridIntegrated = 1u; }, CapabilityRule::both_hybrid},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2000; c.MemoryManagementCaps = 0x10u; c.HybridIntegrated = 1u; }, CapabilityRule::hybrid_integrated_without_outputs},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1200; c.SupportPerEngineTDR = 1u; }, CapabilityRule::per_engine_tdr_without_ddis},
      {[](DxgkDriverCapsRecord &c) { c.PresentationCaps = 0x4u; }, CapabilityRule::kernel_mode_command_buffer_without_render_km},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1200; c.SupportRuntimePowerManagement = 1u; }, CapabilityRule::runtime_power_without_ddis},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1300; c.SupportMultiPlaneOverlay = 1u; }, CapabilityRule::overlay_without_planes},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1300; c.SupportMultiPlaneOverlay = 1u; c.MaxOverlayPlanes = 9u; }, CapabilityRule::too_many_overlay_planes},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2200; c.SupportMultiPlaneOverlay = 1u; c.MaxOverlayPlanes = 11u; }, CapabilityRule::too_many_overlay_planes},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x1300; c.SupportMultiPlaneOverlay = 1u; c.MaxOverlayPlanes = 2u; }, CapabilityRule::overlay_without_ddis},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2300; c.GammaRampCaps = 0x8u; }, CapabilityRule::color_transform_levels},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2300; c.GammaRampCaps = 0x14u; }, CapabilityRule::color_transform_levels},
      {[](DxgkDriverCapsRecord &c) { c.WDDMVersion = 0x2300; c.GammaRampCaps = 0x4u; }, CapabilityRule::color_transform_without_target_gamma},
  };
  for (const Case &c : cases) {
      MockPlatform platform; CraftedMiniport miniport;
      c.mutate(miniport.caps());
      if (miniport.caps().WDDMVersion >= 0x2000) miniport.caps().SchedulingCaps |= 0x1u;
      FakeDxgadapter adapter(platform, miniport.table());
      adapter.validate_wddm_caps();
      CHECK(adapter.validate(miniport.caps(), 0u) == c.rule);
      const std::int32_t expected = c.rule == CapabilityRule::unknown_wddm_version ? revision_mismatch : invalid_parameter;
      CHECK_EQ(adapter.initialize(&miniport, 0u), expected);
      CHECK(adapter.failed_rule() == c.rule);
  } }

TEST(fake_dxgadapter_validate, a_claim_backed_by_its_ddis_is_accepted)
{ MockPlatform platform; CraftedMiniport miniport;
  DxgkDriverCapsRecord &caps = miniport.caps();
  caps.WDDMVersion = 0x2400;
  caps.SchedulingCaps = 0x1u;
  caps.MiscCaps = 0x10u;
  caps.SupportPerEngineTDR = 1u;
  caps.PresentationCaps = 0x4u;
  caps.SupportRuntimePowerManagement = 1u;
  caps.SupportMultiPlaneOverlay = 1u;
  caps.MaxOverlayPlanes = 10u;
  caps.SupportDirectFlip = 1u;
  caps.GammaRampCaps = 0x1Cu;
  caps.MemoryManagementCaps = 0x18010u;
  caps.HybridIntegrated = 1u;
  const DdiSlot slots[] = {
      DdiSlot::DxgkDdiControlInterrupt3, DdiSlot::DxgkDdiQueryDependentEngineGroup,
      DdiSlot::DxgkDdiQueryEngineStatus, DdiSlot::DxgkDdiResetEngine, DdiSlot::DxgkDdiRenderKm,
      DdiSlot::DxgkDdiSetPowerComponentFState, DdiSlot::DxgkDdiPowerRuntimeControlRequest,
      DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay3, DdiSlot::DxgkDdiCheckMultiPlaneOverlaySupport3,
      DdiSlot::DxgkDdiSetTargetGamma, DdiSlot::DxgkDdiQueryDiagnosticTypesSupport,
      DdiSlot::DxgkDdiControlDiagnosticReporting,
  };
  for (const DdiSlot slot : slots) miniport.register_slot(slot);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none);
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0); }

TEST(fake_dxgadapter_validate, full_adapter_rules_and_diagnostic_pairing)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.register_slot(DdiSlot::DxgkDdiCreateDevice);
  miniport.register_slot(DdiSlot::DxgkDdiCommitVidPn);
  DxgkDriverCapsRecord caps = {};
  FakeDxgadapter adapter(platform, miniport.table());
  caps.WDDMVersion = 0x1200;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none);
  adapter.validate_wddm_caps();
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::wddm_1_2_full_adapter_without_direct_flip);
  caps.WDDMVersion = 0x1300;
  caps.SupportDirectFlip = 1u;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::wddm_1_3_full_adapter_without_independent_flip);
  caps.FlipCaps = 0x10u;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none);
  caps.WDDMVersion = 0x1000; caps.SupportDirectFlip = 0u; caps.FlipCaps = 0u;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none);
  caps.WDDMVersion = 0x2400;
  caps.SupportDirectFlip = 1u; caps.FlipCaps = 0x10u;
  miniport.register_slot(DdiSlot::DxgkDdiQueryDiagnosticTypesSupport);
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::diagnostic_ddis_unpaired); }

TEST(fake_dxgadapter_validate, without_validate_wddm_caps_level_claims_pass)
{ MockPlatform platform; CraftedMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  DxgkDriverCapsRecord caps = {};
  caps.WDDMVersion = 0x1000;
  caps.SupportDirectFlip = 1u;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none);
  caps.WDDMVersion = 0x1200;
  caps.FlipCaps = 0x10u;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none);
  caps.WDDMVersion = 0x1300;
  caps.HybridIntegrated = 1u;
  caps.MemoryManagementCaps = 0x10u;
  CHECK(adapter.validate(caps, 1u) == CapabilityRule::none); }

TEST(fake_dxgadapter_initialize, a_wddm_2_driver_is_asked_for_physical_adapter_caps)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x2000;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), invalid_parameter);
  CHECK(adapter.failed_rule() == CapabilityRule::multi_engine_aware_missing);
  miniport.caps().SchedulingCaps = 0x1u;
  miniport.set_nodes(65u);
  CHECK_EQ(adapter.initialize(&miniport, 1u), invalid_parameter);
  CHECK(adapter.failed_rule() == CapabilityRule::too_many_nodes);
  miniport.caps().WDDMVersion = 0x2500;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.queries(), 4u);   }

TEST(fake_dxgadapter_initialize, a_failed_physical_adapter_query_keeps_defaults)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x2000;
  miniport.caps().GpuEngineTopologyNbAsymetricProcessingNodes = 2u;
  miniport.caps().MemoryManagementCapsPagingNode = 1u;
  miniport.caps().MemoryManagementCaps = 0xC0u;
  miniport.fail_query(15, invalid_parameter);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.physical_adapter_caps().NumExecutionNodes, 2u);
  CHECK_EQ(adapter.physical_adapter_caps().PagingNodeIndex, 1u);
  CHECK_EQ(adapter.physical_adapter_caps().Flags, 3u);
  miniport.caps().MemoryManagementCaps = 0u;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.physical_adapter_caps().Flags, 0u); }

TEST(fake_dxgadapter_node_metadata, calls_through_the_table_or_is_a_revision_mismatch)
{ MockPlatform platform; CraftedMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  av::DxgkNodeMetadataRecord metadata = {};
  CHECK_EQ(adapter.node_metadata(&miniport, 0u, metadata), revision_mismatch);
  miniport.set(DdiSlot::DxgkDdiGetNodeMetadata,
               reinterpret_cast<DdiRoutine>(+[](void *, std::uint32_t node, av::DxgkNodeMetadataRecord *m) {
                   m->EngineType = static_cast<std::int32_t>(node) + 1;
                   return 0;
               }));
  CHECK_EQ(adapter.node_metadata(&miniport, 2u, metadata), 0);
  CHECK_EQ(metadata.EngineType, 3); }

TEST(fake_dxgadapter_initialize, io_mmu_secure_mode_asks_for_a_page_multiple_save_size)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x2400;
  miniport.caps().SchedulingCaps = 0x1u;
  miniport.caps().MemoryManagementCaps = 0x800u;
  miniport.set_save_size(0x1001u);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), invalid_parameter);
  CHECK(adapter.failed_rule() == CapabilityRule::frame_buffer_save_size);
  miniport.set_save_size(0x2000u);
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  miniport.fail_query(21, no_memory);
  CHECK_EQ(adapter.initialize(&miniport, 1u), no_memory);
  CHECK(adapter.failed_rule() == CapabilityRule::frame_buffer_save_size);
  miniport.caps().WDDMVersion = 0x1000;
  miniport.caps().SchedulingCaps = 0u;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0); }

TEST(fake_dxgadapter_initialize, native_gpu_fence_requires_its_caps_and_wddm_3_2_is_asked_for_dirty_bits)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x3200;
  miniport.caps().SchedulingCaps = 0x801u;
  miniport.register_slot(DdiSlot::DxgkDdiUpdateMonitorLinkInfo);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.queries(), 6u);
  miniport.fail_query(37, no_memory);
  CHECK_EQ(adapter.initialize(&miniport, 1u), no_memory);
  CHECK(adapter.failed_rule() == CapabilityRule::native_fence_caps_query_failed); }

namespace {

const DdiSlot full_wddm_1_render_set[] = {
    DdiSlot::DxgkDdiCreateDevice, DdiSlot::DxgkDdiDestroyAllocation, DdiSlot::DxgkDdiDescribeAllocation,
    DdiSlot::DxgkDdiOpenAllocation, DdiSlot::DxgkDdiCloseAllocation, DdiSlot::DxgkDdiGetStandardAllocationDriverData,
    DdiSlot::DxgkDdiSubmitCommand, DdiSlot::DxgkDdiPreemptCommand, DdiSlot::DxgkDdiBuildPagingBuffer,
    DdiSlot::DxgkDdiResetFromTimeout, DdiSlot::DxgkDdiRestartFromTimeout, DdiSlot::DxgkDdiQueryCurrentFence,
    DdiSlot::DxgkDdiControlInterrupt, DdiSlot::DxgkDdiDestroyDevice, DdiSlot::DxgkDdiCollectDbgInfo,
    DdiSlot::DxgkDdiPresent, DdiSlot::DxgkDdiCreateAllocation, DdiSlot::DxgkDdiPatch, DdiSlot::DxgkDdiRender,
};

}

TEST(fake_dxgadapter_create_render_core, no_render_ddi_is_a_display_only_adapter)
{ MockPlatform platform; CraftedMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.create_render_core(), 0);
  CHECK(!adapter.render_adapter());
  CHECK(adapter.missing_render_slot() == DdiSlot::Version); }

TEST(fake_dxgadapter_create_render_core, a_partial_set_names_the_first_missing_slot)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.register_slot(DdiSlot::DxgkDdiCreateDevice);
  miniport.register_slot(DdiSlot::DxgkDdiDestroyAllocation);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.create_render_core(), revision_mismatch);
  CHECK(adapter.failed_rule() == CapabilityRule::render_ddis_incomplete);
  CHECK(adapter.missing_render_slot() == DdiSlot::DxgkDdiDescribeAllocation);
  CHECK(!adapter.render_adapter()); }

TEST(fake_dxgadapter_create_render_core, each_required_slot_is_required)
{ for (const DdiSlot left_out : full_wddm_1_render_set) {
      MockPlatform platform; CraftedMiniport miniport;
      for (const DdiSlot slot : full_wddm_1_render_set)
          if (slot != left_out) miniport.register_slot(slot);
      FakeDxgadapter adapter(platform, miniport.table());
      CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
      CHECK_EQ(adapter.create_render_core(), revision_mismatch);
      CHECK(adapter.missing_render_slot() == left_out);
  } }

TEST(fake_dxgadapter_create_render_core, the_full_wddm_1_set_is_a_render_adapter)
{ MockPlatform platform; CraftedMiniport miniport;
  for (const DdiSlot slot : full_wddm_1_render_set) miniport.register_slot(slot);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.create_render_core(), 0);
  CHECK(adapter.render_adapter()); }

TEST(fake_dxgadapter_create_render_core, alternatives_wddm_2_and_compute_only_relax_the_set)
{ MockPlatform platform; CraftedMiniport miniport;
  const DdiSlot relaxed[] = {DdiSlot::DxgkDdiQueryCurrentFence, DdiSlot::DxgkDdiControlInterrupt,
                             DdiSlot::DxgkDdiCollectDbgInfo, DdiSlot::DxgkDdiCreateAllocation,
                             DdiSlot::DxgkDdiPatch, DdiSlot::DxgkDdiRender};
  for (const DdiSlot slot : full_wddm_1_render_set) {
      bool is_relaxed = false;
      for (const DdiSlot candidate : relaxed) is_relaxed = is_relaxed || candidate == slot;
      if (!is_relaxed) miniport.register_slot(slot);
  }
  miniport.register_slot(DdiSlot::DxgkDdiCollectDbgInfo2);
  miniport.caps().WDDMVersion = 0x2000;
  miniport.caps().SchedulingCaps = 0x1u;
  miniport.caps().MiscCaps = 0x8u;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.create_render_core(), 0);
  CHECK(adapter.render_adapter()); }

TEST(fake_dxgadapter_create_render_core, a_wddm_2_render_adapter_needs_flip_on_vsync_mmio)
{ MockPlatform platform; CraftedMiniport miniport;
  for (const DdiSlot slot : full_wddm_1_render_set) miniport.register_slot(slot);
  miniport.caps().WDDMVersion = 0x2000;
  miniport.caps().SchedulingCaps = 0x1u;
  miniport.caps().GpuEngineTopologyNbAsymetricProcessingNodes = 1u;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.create_render_core(), invalid_parameter);
  CHECK(adapter.failed_rule() == CapabilityRule::wddm_2_without_flip_on_vsync_mmio);
  miniport.caps().FlipCaps = 0x2u;
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.create_render_core(), 0); }

TEST(fake_dxgadapter_create_render_core, refuses_swizzling_preemption_and_nodes_it_cannot_honour)
{ struct Case { int knob; std::int32_t status; CapabilityRule rule; };
  const Case cases[] = {
      {0, revision_mismatch, CapabilityRule::swizzling_without_ddis},
      {1, invalid_parameter, CapabilityRule::preemption_without_multi_engine},
      {2, invalid_parameter, CapabilityRule::multi_engine_without_nodes},
  };
  for (const Case &c : cases) {
      MockPlatform platform; CraftedMiniport miniport;
      for (const DdiSlot slot : full_wddm_1_render_set) miniport.register_slot(slot);
      FakeDxgadapter adapter(platform, miniport.table());
      CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
      if (c.knob == 0) miniport.caps().NumberOfSwizzlingRanges = 1u;
      if (c.knob == 1) { miniport.caps().WDDMVersion = 0x1200; miniport.caps().SchedulingCaps = 0x10u; }
      if (c.knob == 2) miniport.caps().SchedulingCaps = 0x1u;
      CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
      CHECK_EQ(adapter.create_render_core(), c.status);
      CHECK(adapter.failed_rule() == c.rule);
  } }
