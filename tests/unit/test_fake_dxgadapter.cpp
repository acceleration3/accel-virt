/* The harness's DXGADAPTER::Initialize model, proven against crafted answers so
 * a later feature that claims a capability without its DDIs fails for the
 * reason dxgkrnl would give. */
#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "../../src/core/ddi_slot.hpp"
#include "../../src/core/ddi_table.hpp"
#include "../../src/core/status.hpp"
#include "../harness/capability_rule.hpp"
#include "../harness/fake_dxgadapter.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"

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

/* A miniport whose QueryAdapterInfo answers from what the test set. */
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

}  // namespace

TEST(fake_dxgadapter_initialize, accepts_wddm_1_0_with_no_capability)
{ MockPlatform platform; CraftedMiniport miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK(adapter.failed_rule() == CapabilityRule::none);
  CHECK_EQ(adapter.driver_caps().WDDMVersion, 0x1000);
  CHECK_EQ(adapter.queries(), 2u);   /* DRIVERCAPS, 64BITONLYCAPS */ }

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

/* ---- DRIVERCAPS rules, one crafted answer each ------------------------------ */

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
      /* A WDDMv2 answer reaches these checks only past MultiEngineAware. */
      if (miniport.caps().WDDMVersion >= 0x2000) miniport.caps().SchedulingCaps |= 0x1u;
      FakeDxgadapter adapter(platform, miniport.table());
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

/* ---- The queries that follow DRIVERCAPS ------------------------------------ */

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
  CHECK_EQ(adapter.queries(), 3u);   /* DRIVERCAPS, PHYSICALADAPTERCAPS, 64BITONLYCAPS */ }

TEST(fake_dxgadapter_initialize, a_failed_physical_adapter_query_keeps_defaults)
{ MockPlatform platform; CraftedMiniport miniport;
  miniport.caps().WDDMVersion = 0x2000;
  miniport.fail_query(15, invalid_parameter);
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0); }

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
  FakeDxgadapter adapter(platform, miniport.table());
  CHECK_EQ(adapter.initialize(&miniport, 1u), 0);
  CHECK_EQ(adapter.queries(), 5u);   /* DRIVERCAPS, PHYSICAL, DIRTYBIT, 64BITONLY, NATIVE_FENCE */
  miniport.fail_query(37, no_memory);
  CHECK_EQ(adapter.initialize(&miniport, 1u), no_memory);
  CHECK(adapter.failed_rule() == CapabilityRule::native_fence_caps_query_failed); }
