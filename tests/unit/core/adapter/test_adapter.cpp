#include "core/adapter/adapter.hpp"
#include "core/adapter/debug_report.hpp"
#include "core/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_child_container_id_record.hpp"
#include "core/ddi/dxgk_child_descriptor_record.hpp"
#include "core/ddi/dxgk_child_status_record.hpp"
#include "core/ddi/dxgk_device_descriptor_record.hpp"
#include "core/ddi/dxgk_display_information_record.hpp"
#include "core/ddi/dxgk_driver_caps_record.hpp"
#include "core/ddi/dxgk_gpu_clock_data_record.hpp"
#include "core/ddi/dxgk_gpu_mmu_caps_record.hpp"
#include "core/ddi/dxgk_node_metadata_record.hpp"
#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgk_page_table_level_desc_record.hpp"
#include "core/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "core/ddi/dxgk_query_page_table_level_desc_in_record.hpp"
#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgkarg_cancel_command_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_create_process_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/ddi/dxgkarg_escape_record.hpp"
#include "core/ddi/dxgkarg_get_root_page_table_size_record.hpp"
#include "core/ddi/dxgkarg_get_scan_line_record.hpp"
#include "core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/ddi/dxgkarg_patch_record.hpp"
#include "core/ddi/dxgkarg_preempt_command_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "core/ddi/dxgkarg_query_dependent_engine_group_record.hpp"
#include "core/ddi/dxgkarg_query_engine_status_record.hpp"
#include "core/ddi/dxgkarg_query_vidpn_hw_capability_record.hpp"
#include "core/ddi/dxgkarg_recommend_functional_vidpn_record.hpp"
#include "core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "core/ddi/dxgkarg_recommend_vidpn_topology_record.hpp"
#include "core/platform/log_budget.hpp"
#include "core/ddi/dxgkarg_render_record.hpp"
#include "core/ddi/dxgkarg_reset_engine_record.hpp"
#include "core/ddi/dxgkarg_reset_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_resume_hw_engine_record.hpp"
#include "core/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_position_record.hpp"
#include "core/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "core/ddi/dxgkarg_set_root_page_table_record.hpp"
#include "core/ddi/dxgkarg_set_stable_power_state_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_address_record.hpp"
#include "core/ddi/dxgkarg_set_vidpn_source_visibility_record.hpp"
#include "core/ddi/dxgkarg_submit_command_record.hpp"
#include "core/ddi/dxgkarg_submit_command_to_hw_queue_record.hpp"
#include "core/ddi/dxgkarg_submit_command_virtual_record.hpp"
#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "core/ddi/linked_device_record.hpp"
#include "core/display/display_mode.hpp"
#include "core/scheduling/context.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/fake_vidpn.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct AdapterProbe {
    explicit AdapterProbe(Adapter &target) : adapter(target) {}
    void set_started(bool started) { adapter.started_ = started; }
    Adapter &adapter;
};

}  // namespace av

using av::Adapter;
using av::AdapterProbe;
using av::DxgkChildDescriptorRecord;
using av::DxgkChildStatusRecord;
using av::DxgkDeviceDescriptorRecord;
using av::DxgkDriverCapsRecord;
using av::DxgkargCollectDbgInfoExtRecord;
using av::DxgkargCollectDbgInfoRecord;
using av::DxgkargEscapeRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::GuardedBuffer;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::LinkedDeviceRecord;
using av::MockPlatform;
using av::Violation;

namespace {

constexpr std::uint32_t success = 0u;
constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::uint32_t no_memory = 0xC0000017u;
constexpr std::uint32_t not_found = 0xC0000225u;
constexpr std::uint32_t no_space = 0xC000009Au;
constexpr std::uint32_t not_supported = 0xC00000BBu;
constexpr std::uint32_t invalid_device_state = 0xC0000184u;
constexpr std::uint32_t buffer_too_small = 0xC0000023u;
constexpr std::uint32_t no_more_descriptor_data = 0xC01D0008u;
constexpr std::uint32_t not_implemented = 0xC0000002u;
constexpr std::int32_t  query_driver_caps = 1;          /* DXGKQAITYPE_DRIVERCAPS */
constexpr std::int32_t  query_wddm_device_caps = 29;    /* DXGKQAITYPE_WDDMDEVICECAPS */
constexpr std::int32_t  last_query_type = 48;           /* DXGKQAITYPE_PAGINGPROCESSGPUVASIZE */

constexpr std::uint32_t adapter_uid = 0xFFFFFFFFu;   /* DISPLAY_ADAPTER_HW_ID */
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
constexpr std::uint8_t  driver_ok_status = 0x0Fu;
constexpr std::uint8_t  failed_bit = 0x80u;

std::uint32_t nt(std::int32_t status) { return static_cast<std::uint32_t>(status); }

/* One adapter as dxgkrnl sees it: AddDevice done, StartDevice not yet. */
class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), adapter_(platform_, &function_), sources_(9u), children_(9u) {}
    ~Fixture() { static_cast<void>(adapter_.stop_device()); }

    std::uint32_t start()
    {
        return nt(adapter_.start_device(nullptr, &dxgkrnl_.record(), &sources_, &children_));
    }

    MockPlatform          &platform() { return platform_; }
    FakeVirtioGpuFunction &function() { return function_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    Adapter               &adapter() { return adapter_; }
    std::uint32_t          sources() const { return sources_; }
    std::uint32_t          children() const { return children_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    Adapter               adapter_;
    std::uint32_t         sources_;
    std::uint32_t         children_;
};

}  // namespace

TEST(adapter, remembers_its_physical_device_object)
{ MockPlatform platform; int pdo = 0;
  const Adapter adapter(platform, &pdo);
  CHECK(adapter.physical_device_object() == &pdo);
  CHECK(adapter.invariants_hold()); }

/* ---- StartDevice ---------------------------------------------------------- */

/* US1 acceptance 2 asks the guest log to show the driver's steps in order, so
 * each line is asserted where it is written rather than only counted. Feature
 * 036's live run stopped after DriverEntry and said nothing more. */
TEST(adapter_start_device, logs_the_step_it_reached)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: StartDevice") == 0);
  av::DxgkChildDescriptorRecord relations = {};
  CHECK_EQ(nt(fixture.adapter().query_child_relations(&relations, sizeof relations)), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: QueryChildRelations") == 0); }

TEST(adapter_start_device, brings_the_device_to_driver_ok_and_reports_one_source_and_child)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.sources(), 1u);
  CHECK_EQ(fixture.children(), 1u);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 1u);
  CHECK_EQ(fixture.function().common().queue(0u).vector, 1u); }

TEST(adapter_start_device, a_second_start_is_invalid_device_state)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.start(), invalid_device_state);
  CHECK_EQ(fixture.dxgkrnl().maps(), 1u); }

TEST(adapter_start_device, missing_arguments_are_invalid_parameters)
{ Fixture fixture; std::uint32_t count = 0u;
  CHECK_EQ(nt(fixture.adapter().start_device(nullptr, nullptr, &count, &count)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().start_device(nullptr, &fixture.dxgkrnl().record(), nullptr, &count)),
           invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().start_device(nullptr, &fixture.dxgkrnl().record(), &count, nullptr)),
           invalid_parameter); }

TEST(adapter_start_device, each_refused_input_fails_with_its_status_and_leaves_nothing_behind)
{ struct Case { int knob; std::uint32_t expected; };
  const Case cases[] = {
      {0, no_memory},             /* DxgkCbGetDeviceInformation refused */
      {1, invalid_parameter},     /* config space read refused */
      {2, invalid_parameter},     /* short config read: malformed */
      {3, no_space},              /* BAR mapping refused */
      {4, not_found},             /* no resource for the virtio BAR */
      {5, invalid_parameter},     /* malformed capability list */
      {6, not_supported},         /* device without VERSION_1 */
      {7, invalid_parameter},     /* less system memory than a page */
  };
  for (const Case &c : cases) {
      Fixture fixture;
      if (c.knob == 0) fixture.dxgkrnl().refuse_device_information(no_memory);
      if (c.knob == 1) fixture.dxgkrnl().refuse_config_read(invalid_parameter);
      if (c.knob == 2) fixture.dxgkrnl().short_config_read();
      if (c.knob == 3) fixture.dxgkrnl().refuse_map(1u, no_space);
      if (c.knob == 4) fixture.function().remove_virtio_bar_resource();
      if (c.knob == 5) fixture.function().config_space()[0x34u] = 0x20u;
      if (c.knob == 6) fixture.function().common().offer_features(FakeVirtioGpuFunction::feature_edid);
      if (c.knob == 7) fixture.dxgkrnl().set_system_memory(0);
      CHECK_EQ(fixture.start(), c.expected);
      CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
      CHECK_EQ(fixture.platform().live_allocations(), 0u);
      CHECK(fixture.adapter().invariants_hold());
  } }

TEST(adapter_start_device, a_table_older_than_wddm_3_2_is_not_supported)
{ Fixture fixture;
  fixture.dxgkrnl().record().Size = 0x100u;
  CHECK_EQ(fixture.start(), not_supported); }

TEST(adapter_start_device, a_failed_device_start_leaves_the_device_reset_and_unmapped)
{ Fixture fixture;
  fixture.function().common().set_queues(1u, 256u);
  CHECK_EQ(fixture.start(), not_found);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(fixture.platform().live_allocations(), 0u);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success); }

TEST(adapter_start_device, each_allocation_failing_in_turn_fails_cleanly_then_starts)
{ for (std::uint32_t nth = 1u; nth <= 8u; nth++) {
      Fixture fixture;
      fixture.platform().fail_allocation_at(nth);
      const std::uint32_t status = fixture.start();
      CHECK(status == success || status == no_memory);
      fixture.platform().fail_allocation_at(0u);
      if (status == no_memory) {
          CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
          CHECK_EQ(fixture.platform().live_allocations(), 0u);
          CHECK_EQ(fixture.start(), success);
      }
  } }

TEST(adapter_start_device, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.start());
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {} }

/* ---- StopDevice and teardown ---------------------------------------------- */

TEST(adapter_stop_device, resets_the_device_and_releases_every_mapping)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(fixture.platform().live_allocations(), 0u);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_stop_device, a_second_stop_and_a_stop_before_start_are_harmless)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success); }

TEST(adapter_stop_device, a_device_that_ignores_reset_is_reported_and_still_released)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.function().common().delay_reset(100000u);
  CHECK_EQ(nt(fixture.adapter().stop_device()), invalid_device_state);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(fixture.platform().live_allocations(), 0u); }

TEST(adapter_stop_device, restarts_after_stop)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status); }

TEST(adapter, destruction_without_stop_releases_everything)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  { FakeDxgkrnlAdapter dxgkrnl(platform, function);
    { Adapter adapter(platform, &function);
      std::uint32_t sources = 0u, children = 0u;
      CHECK_EQ(nt(adapter.start_device(nullptr, &dxgkrnl.record(), &sources, &children)), success); }
    CHECK_EQ(dxgkrnl.live_mappings(), 0u);
    CHECK_EQ(function.common().status(), 0u); }
  CHECK_EQ(platform.live_allocations(), 0u); }

/* ---- Power ----------------------------------------------------------------- */

TEST(adapter_set_power_state, d3_resets_the_device_and_d0_brings_it_back)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 1u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_set_power_state, a_child_transition_needs_nothing)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().set_power_state(0u, power_d3, 0)), success); }

TEST(adapter_set_power_state, before_start_is_invalid_device_state)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), invalid_device_state); }

TEST(adapter_set_power_state, an_unspecified_or_unknown_state_is_invalid)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, 0, 0)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, 5, 0)), invalid_parameter);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status); }

TEST(adapter_set_power_state, a_device_that_will_not_come_back_is_reported)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  fixture.function().common().set_queues(1u, 256u);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), not_found);
  CHECK_EQ(fixture.platform().live_allocations(), 1u);   /* the cursor's backing, held while started */ }

/* ---- ResetDevice ----------------------------------------------------------- */

TEST(adapter_reset_device, writes_reset_at_any_irql_without_allocating)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint32_t allocations = fixture.platform().allocations_made();
  fixture.platform().set_irql(Irql::isr);
  fixture.adapter().reset_device();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.platform().allocations_made(), allocations);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter_reset_device, before_start_touches_nothing)
{ Fixture fixture;
  fixture.adapter().reset_device();
  CHECK_EQ(fixture.function().common().status_writes(), 0u); }

/* ---- Interrupt and DPC ----------------------------------------------------- */

TEST(adapter_interrupt_routine, a_message_interrupt_is_claimed_and_queues_a_dpc)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(1u), 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u); }

TEST(adapter_interrupt_routine, a_line_interrupt_is_claimed_only_with_isr_bits_set)
{ Fixture fixture;
  fixture.function().set_message_interrupts(0u);
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.function().isr().raise(0x1u);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u); }

TEST(adapter_interrupt_routine, an_interrupt_before_start_is_not_claimed)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 0u); }

TEST(adapter_dpc_routine, harvests_the_control_queue_at_dispatch)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter_dpc_routine, before_start_or_after_power_down_does_nothing)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  const std::uint32_t quiet = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), quiet);   /* a clean DPC says nothing */
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 1u); }

TEST(adapter_dpc_routine, a_device_lying_about_a_completion_is_failed_and_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(0u);
  const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
  used[2] = 1u;   /* used.idx: one completion nothing was submitted for */
  const std::uint32_t logged = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().logged() > logged);
  CHECK((fixture.function().common().status() & failed_bit) != 0u); }

/* ---- Children -------------------------------------------------------------- */

TEST(adapter_query_child_relations, reports_one_hot_pluggable_video_output)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildDescriptorRecord relations[2];
  std::memset(relations, 0xEE, sizeof relations);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(relations, sizeof relations)), success);
  CHECK_EQ(relations[0].ChildDeviceType, 1);              /* TypeVideoOutput */
  CHECK_EQ(relations[0].InterfaceTechnology, -1);         /* D3DKMDT_VOT_OTHER */
  CHECK_EQ(relations[0].MonitorOrientationAwareness, 1);  /* D3DKMDT_MOA_NONE */
  CHECK_EQ(relations[0].SupportsSdtvModes, 0u);
  CHECK_EQ(relations[0].HpdAwareness, 4);                 /* HpdAwarenessInterruptible */
  CHECK_EQ(relations[0].AcpiUid, 0u);
  CHECK_EQ(relations[0].ChildUid, 0u);
  CHECK_EQ(relations[1].ChildDeviceType, static_cast<std::int32_t>(0xEEEEEEEEu)); }

TEST(adapter_query_child_relations, no_room_is_buffer_too_small_and_no_buffer_invalid)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  DxgkChildDescriptorRecord relation;
  CHECK_EQ(nt(fixture.adapter().query_child_relations(&relation, sizeof relation - 1u)), buffer_too_small);
  CHECK_EQ(nt(fixture.adapter().query_child_relations(nullptr, sizeof relation)), invalid_parameter); }

TEST(adapter_query_child_status, the_one_child_is_connected)
{ Fixture fixture;
  DxgkChildStatusRecord status = {};
  status.Type = 1;   /* StatusConnection */
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 1u)), success);
  CHECK_EQ(status.HotPlugConnected, 1u); }

TEST(adapter_query_child_status, rotation_other_types_and_other_children_are_refused)
{ Fixture fixture;
  DxgkChildStatusRecord status = {};
  status.Type = 2;   /* StatusRotation */
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), invalid_parameter);
  status.Type = 3;   /* StatusMiracastConnection */
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), not_supported);
  status.Type = 1;
  status.ChildUid = 1u;
  CHECK_EQ(nt(fixture.adapter().query_child_status(&status, 0u)), invalid_parameter);
  CHECK_EQ(status.HotPlugConnected, 0u);
  CHECK_EQ(nt(fixture.adapter().query_child_status(nullptr, 0u)), invalid_parameter); }

TEST(adapter_query_device_descriptor, answers_the_synthetic_monitor_edid_for_the_one_child)
{ Fixture fixture;
  std::uint8_t edid[128] = {};
  DxgkDeviceDescriptorRecord descriptor = {0u, sizeof edid, edid};
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), success);
  CHECK_EQ(descriptor.DescriptorLength, 128u);
  CHECK_EQ(edid[1], 0xFFu);
  descriptor.DescriptorOffset = 128u;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, &descriptor)), no_more_descriptor_data); }

TEST(adapter_query_device_descriptor, another_child_or_no_descriptor_is_invalid)
{ Fixture fixture;
  std::uint8_t edid[128] = {};
  DxgkDeviceDescriptorRecord descriptor = {0u, sizeof edid, edid};
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(1u, &descriptor)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, nullptr)), invalid_parameter);
  CHECK_EQ(edid[0], 0u); }

/* ---- VidPn and modes -------------------------------------------------------- */

namespace {

constexpr std::uint32_t invalid_source_mode = 0xC01E0310u;
constexpr std::uint32_t transformation_not_supported = 0xC01E0346u;
constexpr std::uint32_t no_recommended_functional_vidpn = 0xC01E0323u;
constexpr std::uint32_t no_recommended_vidpn_topology = 0xC01E031Au;
constexpr std::int32_t  interrupt_crtc_vsync = 3;
constexpr std::int32_t  interrupt_dpc_log = 4;   /* DXGK_INTERRUPT_DPC_LOG_TYPE: anything else */
constexpr std::int32_t  no_pivot = 5;

av::D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    av::D3dkmdtVidPnSourceModeRecord mode = {};
    av::DisplayMode(width, height).describe(mode);
    return mode;
}

/* Commits `width` x `height` to source 0 through a fake VidPn. */
std::uint32_t commit(Fixture &fixture, std::uint32_t width, std::uint32_t height)
{
    av::FakeVidPn vidpn;
    vidpn.add_path(0u, 0u);
    vidpn.source_modes(0u).seed(source_mode(width, height), true);
    const av::DxgkargCommitVidPnRecord request = {&vidpn, 0u, 0, nullptr, 0u, 0u};
    return nt(fixture.adapter().commit_vidpn(&request));
}

}  // namespace

TEST(adapter_is_supported_vidpn, answers_for_the_proposed_vidpn)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 1u);
  av::DxgkargIsSupportedVidPnRecord request = {&vidpn, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().is_supported_vidpn(&request)), success);
  CHECK_EQ(request.IsVidPnSupported, 0u);
  CHECK_EQ(nt(fixture.adapter().is_supported_vidpn(nullptr)), invalid_parameter); }

TEST(adapter_enum_vidpn_cofunc_modality, offers_the_modes_on_the_path)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), success);
  CHECK_EQ(vidpn.source_modes(0u).modes(), 11u);
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(nullptr)), invalid_parameter); }

TEST(adapter_enum_vidpn_cofunc_modality, before_start_dxgkrnl_cannot_be_reached)
{ Fixture fixture;
  av::FakeVidPn vidpn;
  const av::DxgkargEnumVidPnCofuncModalityRecord request = {&vidpn, no_pivot, 0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().enum_vidpn_cofunc_modality(&request)), invalid_device_state); }

TEST(adapter_commit_vidpn, records_the_mode_scan_lines_follow)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(commit(fixture, 1920u, 1080u), success);
  av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
  const std::uint64_t second = 10000000u;
  fixture.platform().advance_time(second - fixture.platform().interrupt_time() % second + 163000u);
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), success);
  CHECK_EQ(scan.ScanLine, 1100u);
  CHECK_EQ(scan.InVerticalBlank, 1u);
  fixture.platform().advance_time(second - 163000u + 83333u);
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), success);
  CHECK_EQ(scan.ScanLine, 562u);
  CHECK_EQ(scan.InVerticalBlank, 0u);
  CHECK_EQ(commit(fixture, 1000u, 700u), invalid_source_mode);
  CHECK_EQ(nt(fixture.adapter().commit_vidpn(nullptr)), invalid_parameter); }

TEST(adapter_get_scan_line, needs_a_committed_mode_on_the_one_target)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargGetScanLineRecord scan = {0u, 0u, {}, 0u};
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), invalid_device_state);
  CHECK_EQ(commit(fixture, 640u, 480u), success);
  scan.VidPnTargetId = 1u;
  CHECK_EQ(nt(fixture.adapter().get_scan_line(&scan)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_scan_line(nullptr)), invalid_parameter); }

TEST(adapter_update_active_vidpn_present_path, accepts_identity_transformation_only)
{ Fixture fixture;
  av::D3dkmdtVidPnPresentPathRecord path = {};
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), success);
  path.ContentTransformationScaling = 2;   /* D3DKMDT_VPPS_CENTERED */
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), transformation_not_supported);
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 2;  /* D3DKMDT_VPPR_ROTATE90 */
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), transformation_not_supported); }

TEST(adapter_update_active_vidpn_present_path, a_path_the_adapter_lacks_is_invalid)
{ Fixture fixture;
  av::D3dkmdtVidPnPresentPathRecord path = {};
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  path.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), invalid_parameter);
  path.VidPnSourceId = 0u;
  path.VidPnTargetId = 1u;
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(&path)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().update_active_vidpn_present_path(nullptr)), invalid_parameter); }

TEST(adapter_recommend_monitor_modes, fills_the_monitor_mode_set)
{ Fixture fixture;
  av::FakeVidPn vidpn;
  const av::DxgkargRecommendMonitorModesRecord request = {0u, 0u, vidpn.monitor_mode_set(), &vidpn.monitor_interface()};
  CHECK_EQ(nt(fixture.adapter().recommend_monitor_modes(&request)), success);
  CHECK_EQ(vidpn.monitor_modes().modes(), 11u);
  CHECK_EQ(nt(fixture.adapter().recommend_monitor_modes(nullptr)), invalid_parameter); }

TEST(adapter, recommend_functional_vidpn_and_recommend_vidpn_topology_decline)
{ Fixture fixture;
  const av::DxgkargRecommendFunctionalVidPnRecord functional = {};
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  CHECK_EQ(nt(fixture.adapter().recommend_functional_vidpn(&functional)), no_recommended_functional_vidpn);
  CHECK_EQ(nt(fixture.adapter().recommend_vidpn_topology(&topology)), no_recommended_vidpn_topology); }

TEST(adapter_control_interrupt, vsync_runs_between_enable_and_disable)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint64_t frame = 10000000u / 60u;
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  fixture.platform().run_timers(frame * 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).InterruptType, interrupt_crtc_vsync);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 0u)), success);
  fixture.platform().run_timers(frame * 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_dpc_log, 1u)), not_supported); }

TEST(adapter_control_interrupt, before_start_there_is_no_vsync)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 0u)), success); }

TEST(adapter_stop_device, stops_the_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  fixture.platform().run_timers(10000000u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u); }

TEST(adapter_set_vidpn_source_address, the_vsync_reports_the_primary_it_names)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.PrimarySegment = 1u;
  request.PrimaryAddress = 0x7000000000ll;
  request.hAllocation = &primary;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  fixture.platform().run_timers(10000000u / 60u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0x70u);
  request.hAllocation = nullptr;
  request.PrimarySegment = 9u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().run_timers(10000000u / 60u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).Argument3, 0u); }

TEST(adapter_set_vidpn_source_address, an_immediate_flip_is_not_reported_and_shows_at_the_next_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.PrimarySegment = 1u;
  request.PrimaryAddress = 0x5000000000ll;
  request.hAllocation = &primary;
  request.Flags = 0x2u;   /* FlipImmediate */
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.platform().run_timers(10000000u / 60u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0x50u); }

TEST(adapter_set_vidpn_source_address, a_primary_in_system_memory_names_segment_zero)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt(interrupt_crtc_vsync, 1u)), success);
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.hAllocation = &primary;   /* VIDMM_SEGMENT::DriverId answers 0 for the implicit segment, address 0 */
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), success);
  fixture.platform().set_irql(Irql::passive);
  fixture.platform().run_timers(10000000u / 60u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument2, 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0u); }

TEST(adapter_set_vidpn_source_address, another_segment_or_source_is_invalid)
{ Fixture fixture;
  int primary = 0;
  av::DxgkargSetVidPnSourceAddressRecord request = {};
  request.hAllocation = &primary;
  request.PrimarySegment = 2u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), invalid_parameter);
  request.PrimarySegment = 1u;
  request.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address(nullptr)), invalid_parameter); }

TEST(adapter_set_vidpn_source_visibility, records_it_for_the_source)
{ Fixture fixture;
  av::DxgkargSetVidPnSourceVisibilityRecord request = {0u, 1u, {}};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&request)), success);
  request.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(nullptr)), invalid_parameter); }

TEST(adapter_present, needs_a_live_context_then_presents)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint8_t dma[128] = {};
  av::D3dddiPatchLocationListRecord patches[2] = {};
  av::DxgkargPresentRecord present = {};
  present.pDmaBuffer = dma;
  present.DmaSize = sizeof dma;
  present.pPatchLocationListOut = patches;
  present.PatchLocationListOutSize = 2u;
  present.Flags = 1u;   /* Blt */
  CHECK_EQ(nt(fixture.adapter().present(&fixture, &present)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().present(context.hContext, &present)), success);
  CHECK_EQ(dma[0], 0x52u);
  CHECK_EQ(patches[1].AllocationIndex, 2u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter, the_display_ddis_are_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().is_supported_vidpn(nullptr));
  static_cast<void>(fixture.adapter().recommend_functional_vidpn(nullptr));
  static_cast<void>(fixture.adapter().enum_vidpn_cofunc_modality(nullptr));
  static_cast<void>(fixture.adapter().commit_vidpn(nullptr));
  static_cast<void>(fixture.adapter().update_active_vidpn_present_path(nullptr));
  static_cast<void>(fixture.adapter().recommend_monitor_modes(nullptr));
  static_cast<void>(fixture.adapter().recommend_vidpn_topology(nullptr));
  static_cast<void>(fixture.adapter().get_scan_line(nullptr));
  static_cast<void>(fixture.adapter().control_interrupt(0, 0u));
  static_cast<void>(fixture.adapter().set_vidpn_source_visibility(nullptr));
  static_cast<void>(fixture.adapter().present(nullptr, nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 11; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- WDDM 1.3 (feature 016) -------------------------------------------------- */

TEST(adapter_get_node_metadata, one_3d_node_named_as_basicrender_names_it)
{ Fixture fixture;
  av::DxgkNodeMetadataRecord metadata;
  std::memset(&metadata, 0xEE, sizeof metadata);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0u, &metadata)), success);
  CHECK_EQ(metadata.EngineType, 1);
  const char expected[] = "3DNode00";
  for (unsigned index = 0u; index < sizeof expected; index++)
      CHECK_EQ(metadata.FriendlyName[index], static_cast<unsigned>(expected[index]));
  CHECK_EQ(metadata.FriendlyName[31], 0u);
  CHECK_EQ(metadata.Flags, 0u);
  CHECK_EQ(metadata.GpuMmuSupported, 1u);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(1u, &metadata)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0x10000u, &metadata)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_node_metadata(0u, nullptr)), invalid_parameter); }

TEST(adapter_control_interrupt2, enables_and_disables_the_synthetic_vsync)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint64_t frame = 10000000u / 60u;
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 0})), success);
  fixture.platform().run_timers(frame);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 1})), success);
  fixture.platform().run_timers(frame * 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 2})), success);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, 3})), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_crtc_vsync, -1})), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().control_interrupt2({interrupt_dpc_log, 0})), not_supported); }

TEST(adapter_calibrate_gpu_clock, the_gpu_clock_is_the_performance_counter)
{ Fixture fixture;
  fixture.platform().advance_time(4242u);
  av::DxgkGpuClockDataRecord clock;
  std::memset(&clock, 0xEE, sizeof clock);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(0u, 0u, &clock)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(clock.GpuFrequency, 10000000u);
  CHECK_EQ(clock.GpuClockCounter, fixture.platform().interrupt_time());
  CHECK_EQ(clock.CpuClockCounter, clock.GpuClockCounter);
  CHECK_EQ(clock.Flags, 0u);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(1u, 0u, &clock)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(0u, 1u, &clock)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().calibrate_gpu_clock(0u, 0u, nullptr)), invalid_parameter); }

TEST(adapter, the_wddm_1_3_ddis_declare_their_ceilings)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().set_power_pstate(0u, 0u)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().get_node_metadata(0u, nullptr));
  static_cast<void>(fixture.adapter().set_power_pstate(0u, 0u));
  static_cast<void>(fixture.adapter().control_interrupt2({interrupt_dpc_log, 0}));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.adapter().calibrate_gpu_clock(0u, 0u, nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 4; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- WDDM 1.2 (feature 015) -------------------------------------------------- */

TEST(adapter_reset_engine, aborts_what_the_node_was_executing_and_keeps_it_running)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  fixture.platform().set_irql(Irql::dispatch);
  for (std::uint32_t fence = 1u; fence <= 2u; fence++) {
      submit.SubmissionFenceId = fence;
      CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  }
  fixture.platform().set_irql(Irql::passive);
  av::DxgkargResetEngineRecord reset = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_engine(&reset)), success);
  CHECK_EQ(reset.LastAbortedFenceId, 1u);
  submit.SubmissionFenceId = 3u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  const av::DxgkargResetEngineRecord refused[] = {{1u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargResetEngineRecord &request : refused) {
      av::DxgkargResetEngineRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().reset_engine(&copy)), invalid_parameter);
  }
  CHECK_EQ(nt(fixture.adapter().reset_engine(nullptr)), invalid_parameter); }

TEST(adapter_query_engine_status, the_one_engine_is_responsive)
{ Fixture fixture;
  av::DxgkargQueryEngineStatusRecord status = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_engine_status(&status)), success);
  CHECK_EQ(status.EngineStatus, 1u);
  const av::DxgkargQueryEngineStatusRecord refused[] = {{1u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargQueryEngineStatusRecord &request : refused) {
      av::DxgkargQueryEngineStatusRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().query_engine_status(&copy)), invalid_parameter);
      CHECK_EQ(copy.EngineStatus, 0u);
  }
  CHECK_EQ(nt(fixture.adapter().query_engine_status(nullptr)), invalid_parameter); }

TEST(adapter_query_dependent_engine_group, the_one_node_depends_on_itself)
{ Fixture fixture;
  av::DxgkargQueryDependentEngineGroupRecord group = {0u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(&group)), success);
  CHECK_EQ(group.DependentNodeOrdinalMask, 1u);
  const av::DxgkargQueryDependentEngineGroupRecord refused[] = {{1u, 0u, 0u}, {0u, 1u, 0u}};
  for (const av::DxgkargQueryDependentEngineGroupRecord &request : refused) {
      av::DxgkargQueryDependentEngineGroupRecord copy = request;
      CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(&copy)), invalid_parameter);
  }
  CHECK_EQ(nt(fixture.adapter().query_dependent_engine_group(nullptr)), invalid_parameter); }

TEST(adapter_cancel_command, succeeds_for_a_live_context_only)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::DxgkargCancelCommandRecord cancel = {};
  cancel.hContext = context.hContext;
  CHECK_EQ(nt(fixture.adapter().cancel_command(&cancel)), success);
  cancel.hContext = &fixture;
  CHECK_EQ(nt(fixture.adapter().cancel_command(&cancel)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().cancel_command(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_notify_surprise_removal, nothing_touches_the_device_afterwards)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(1)), success);
  fixture.function().common().delay_reset(100000u);   /* a gone device never acknowledges */
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(fixture.adapter().interrupt_routine(0u), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  const std::uint64_t before = fixture.platform().interrupt_time();
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.platform().interrupt_time(), before);
  CHECK_EQ(fixture.platform().live_allocations(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u); }

TEST(adapter_notify_surprise_removal, a_new_start_finds_the_device_present_again)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(0)), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(nt(fixture.adapter().notify_surprise_removal(2)), invalid_parameter); }

TEST(adapter_stop_device_and_release_post_display_ownership, stops_and_hands_over_no_frame_buffer)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkDisplayInformationRecord display;
  std::memset(&display, 0xEE, sizeof display);
  CHECK_EQ(nt(fixture.adapter().stop_device_and_release_post_display_ownership(1u, &display)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().stop_device_and_release_post_display_ownership(0u, nullptr)), invalid_parameter);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  CHECK_EQ(nt(fixture.adapter().stop_device_and_release_post_display_ownership(0u, &display)), success);
  CHECK_EQ(display.Width, 0u);
  CHECK(display.PhysicAddress == 0);
  CHECK_EQ(fixture.function().common().status(), 0u);
  CHECK_EQ(fixture.dxgkrnl().live_mappings(), 0u); }

TEST(adapter_get_child_container_id, describes_the_synthetic_monitor)
{ Fixture fixture;
  av::DxgkChildContainerIdRecord container = {};
  CHECK_EQ(nt(fixture.adapter().get_child_container_id(0u, &container)), success);
  CHECK_EQ(container.EldInfoProductCode, 1u);
  CHECK_EQ(nt(fixture.adapter().get_child_container_id(1u, &container)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_child_container_id(0u, nullptr)), invalid_parameter); }

TEST(adapter, system_display_power_components_and_overlays_decline)
{ Fixture fixture;
  std::uint32_t width = 7u, height = 7u;
  std::int32_t format = 7;
  CHECK_EQ(nt(fixture.adapter().system_display_enable(0u, nullptr, &width, &height, &format)), not_supported);
  CHECK_EQ(width, 7u);
  fixture.adapter().system_display_write(nullptr, 1u, 1u, 4u, 0u, 0u);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().set_power_component_fstate(0u, 1u)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().power_runtime_control_request(nullptr, nullptr, 0u, nullptr, 0u, nullptr)),
           not_supported);
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_address_with_multi_plane_overlay(nullptr)), not_supported);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(adapter, the_win8_ddis_declare_their_ceilings)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().notify_surprise_removal(9));
  static_cast<void>(fixture.adapter().stop_device_and_release_post_display_ownership(9u, nullptr));
  static_cast<void>(fixture.adapter().get_child_container_id(9u, nullptr));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.adapter().set_power_component_fstate(0u, 0u));
  static_cast<void>(fixture.adapter().power_runtime_control_request(nullptr, nullptr, 0u, nullptr, 0u, nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 5; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

TEST(adapter, the_engine_ddis_are_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().query_dependent_engine_group(nullptr));
  static_cast<void>(fixture.adapter().query_engine_status(nullptr));
  static_cast<void>(fixture.adapter().reset_engine(nullptr));
  static_cast<void>(fixture.adapter().cancel_command(nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 4; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- WDDM 1.1 (feature 014) -------------------------------------------------- */

TEST(adapter_query_vidpn_hw_capability, the_driver_transforms_nothing_on_the_one_path)
{ Fixture fixture;
  int vidpn = 0;
  av::DxgkargQueryVidPnHwCapabilityRecord query = {&vidpn, 0u, 0u, 0xFFFFFFFFu, 0u};
  CHECK_EQ(nt(fixture.adapter().query_vidpn_hw_capability(&query)), success);
  CHECK_EQ(query.VidPnHWCaps, 0u); }

TEST(adapter_query_vidpn_hw_capability, another_path_or_no_vidpn_is_invalid)
{ Fixture fixture;
  int vidpn = 0;
  const av::DxgkargQueryVidPnHwCapabilityRecord refused[] = {
      {nullptr, 0u, 0u, 7u, 0u}, {&vidpn, 1u, 0u, 7u, 0u}, {&vidpn, 0u, 1u, 7u, 0u}};
  for (const av::DxgkargQueryVidPnHwCapabilityRecord &query : refused) {
      av::DxgkargQueryVidPnHwCapabilityRecord copy = query;
      CHECK_EQ(nt(fixture.adapter().query_vidpn_hw_capability(&copy)), invalid_parameter);
      CHECK_EQ(copy.VidPnHWCaps, 7u);
  }
  CHECK_EQ(nt(fixture.adapter().query_vidpn_hw_capability(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().query_vidpn_hw_capability(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- Cursor and the legacy surface (feature 013) ---------------------------- */

TEST(adapter_set_pointer_shape, reaches_the_device_once_started)
{ Fixture fixture;
  const std::uint8_t pixels[4] = {1, 2, 3, 0xFF};
  const av::DxgkargSetPointerShapeRecord shape = {2u, 1u, 1u, 4u, 0u, 0u, pixels, 0u, 0u};
  const av::DxgkargSetPointerPositionRecord position = {0u, 10, 20, 1u};
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(&position)), invalid_device_state);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(&position)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_pointer_position(nullptr)), invalid_parameter);
  const av::FakeCommonConfig::Queue &control = fixture.function().common().queue(0u);
  av::FakeVirtqueueDevice device(fixture.platform(), control.desc, control.driver, control.device, control.size);
  CHECK_EQ(device.respond_all(0x1100u), 3u);
  const std::uint32_t quiet = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), quiet); }   /* a clean DPC says nothing */

TEST(adapter_set_pointer_shape, a_power_cycle_creates_the_cursor_resource_again)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::uint8_t pixels[4] = {1, 2, 3, 0xFF};
  const av::DxgkargSetPointerShapeRecord shape = {2u, 1u, 1u, 4u, 0u, 0u, pixels, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), invalid_device_state);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d0, 0)), success);
  CHECK_EQ(nt(fixture.adapter().set_pointer_shape(&shape)), success);
  const av::FakeCommonConfig::Queue &control = fixture.function().common().queue(0u);
  av::FakeVirtqueueDevice device(fixture.platform(), control.desc, control.driver, control.device, control.size);
  CHECK_EQ(device.respond_all(0x1100u), 3u);
  CHECK_EQ(device.request_type(0u), 0x0101u); }

TEST(adapter, the_legacy_surface_answers_not_supported)
{ Fixture fixture;
  int request = 0;
  CHECK_EQ(nt(fixture.adapter().acquire_swizzling_range(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().release_swizzling_range(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().set_palette(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().stop_capture(&request)), not_supported);
  CHECK_EQ(nt(fixture.adapter().create_overlay(&request)), not_supported);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().acquire_swizzling_range(nullptr));
  static_cast<void>(fixture.adapter().release_swizzling_range(nullptr));
  static_cast<void>(fixture.adapter().set_palette(nullptr));
  static_cast<void>(fixture.adapter().stop_capture(nullptr));
  static_cast<void>(fixture.adapter().create_overlay(nullptr));
  static_cast<void>(fixture.adapter().set_pointer_shape(nullptr));
  static_cast<void>(fixture.adapter().set_pointer_position(nullptr));
  fixture.platform().set_irql(Irql::passive);
  for (int call = 0; call < 7; call++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- The rest of the slot list --------------------------------------------- */

TEST(adapter_link_device, is_a_chain_of_one_led_by_this_adapter)
{ Fixture fixture;
  LinkedDeviceRecord linked;
  std::memset(&linked, 0xEE, sizeof linked);
  CHECK_EQ(nt(fixture.adapter().link_device(&fixture.function(), &linked)), success);
  CHECK_EQ(linked.ChainUid, 0u);
  CHECK_EQ(linked.NumberOfLinksInChain, 1u);
  CHECK_EQ(linked.LeadLink, 1u);
  CHECK_EQ(nt(fixture.adapter().link_device(&fixture.function(), nullptr)), invalid_parameter); }

TEST(adapter, dispatch_io_request_and_query_interface_are_not_supported)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().dispatch_io_request(0u, nullptr)), not_supported);
  CHECK_EQ(nt(fixture.adapter().query_interface(nullptr)), not_supported); }

TEST(adapter_notify_acpi_event, succeeds_and_asks_for_nothing)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  std::uint32_t flags = 3u;
  CHECK_EQ(adapter.notify_acpi_event(0, 0u, nullptr, &flags), 0);
  CHECK_EQ(flags, 0u); }

TEST(adapter_invariants, a_powered_or_mapped_adapter_that_is_not_started_is_detected)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  AdapterProbe probe(fixture.adapter());
  probe.set_started(false);
  CHECK(!fixture.adapter().invariants_hold());
  probe.set_started(true);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  probe.set_started(false);
  CHECK(!fixture.adapter().invariants_hold());
  probe.set_started(true); }

/* ---- QueryAdapterInfo ------------------------------------------------------ */

namespace {

/* What DXGADAPTER::DdiQueryAdapterInfo accepts without logging an invalid status. */
bool accepted_by_dxgkrnl(std::uint32_t status)
{
    return status == success || status == not_implemented || status == invalid_parameter || status == no_memory;
}

std::uint32_t query_driver_caps_of(Adapter &adapter, DxgkDriverCapsRecord &caps)
{
    DxgkargQueryAdapterInfoRecord query = {};
    query.Type = query_driver_caps;
    query.pOutputData = &caps;
    query.OutputDataSize = sizeof caps;
    return nt(adapter.query_adapter_info(&query));
}

}  // namespace

TEST(adapter_query_adapter_info, answers_driver_caps_with_wddm_3_2)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK_EQ(caps.WDDMVersion, 0x3200); }

/* dxgkrnl asks for types the driver declines, so the line carries the status
 * as well as the type: in the guest that is how the order of the capability
 * queries, and which of them were refused, is read back (US1 acceptance 2). */
TEST(adapter_query_adapter_info, logs_each_type_with_the_status_it_answered)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: QueryAdapterInfo type 1 status 0x00000000") == 0);
  DxgkargQueryAdapterInfoRecord declined = {};
  declined.Type = 999;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&declined)), not_implemented);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: QueryAdapterInfo type 999 status 0xC0000002") == 0); }

TEST(adapter_query_adapter_info, answers_the_gpu_mmu_and_physical_adapter_queries)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint32_t physical_adapter = 0u;
  av::DxgkGpuMmuCapsRecord mmu = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 13;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &mmu;
  query.OutputDataSize = sizeof mmu;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(mmu.PageTableLevelCount, 4u);
  av::DxgkQueryPageTableLevelDescInRecord level_in = {3u, 0u};
  av::DxgkPageTableLevelDescRecord level = {};
  query.Type = 14;
  query.pInputData = &level_in;
  query.InputDataSize = sizeof level_in;
  query.pOutputData = &level;
  query.OutputDataSize = sizeof level;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(level.PageTableIndexBitCount, 9u);
  av::DxgkPhysicalAdapterCapsRecord physical = {};
  query.Type = 15;
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &physical;
  query.OutputDataSize = sizeof physical;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(physical.Flags, 0x2u);
  CHECK(physical.DxgkPhysicalAdapterHandle == &fixture.dxgkrnl()); }

TEST(adapter_query_adapter_info, driver_caps_do_not_depend_on_the_device_state)
{ Fixture fixture; DxgkDriverCapsRecord before = {}, started = {}, d3 = {}, stopped = {};
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), before), success);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), started), success);
  CHECK_EQ(nt(fixture.adapter().set_power_state(adapter_uid, power_d3, 0)), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), d3), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), stopped), success);
  CHECK(std::memcmp(&before, &started, sizeof before) == 0);
  CHECK(std::memcmp(&before, &d3, sizeof before) == 0);
  CHECK(std::memcmp(&before, &stopped, sizeof before) == 0);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_query_adapter_info, answers_wddm_device_caps_before_start)
{ Fixture fixture; std::uint32_t interface_version = 0x11007u; std::int32_t caps = 0;
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_wddm_device_caps;
  query.pInputData = &interface_version;
  query.InputDataSize = sizeof interface_version;
  query.pOutputData = &caps;
  query.OutputDataSize = sizeof caps;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(caps, 0x3200); }

TEST(adapter_query_adapter_info, answers_the_display_caps_extension)
{ Fixture fixture; std::uint32_t extension = 0xEEu;
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 16;   /* DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION */
  query.pOutputData = &extension;
  query.OutputDataSize = sizeof extension;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(extension, 0u); }

TEST(adapter_query_adapter_info, answers_the_segment_query_once_started)
{ Fixture fixture; std::uint32_t physical_adapter = 0u; av::DxgkQuerySegmentOut4Record out = {};
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = 11;   /* DXGKQAITYPE_QUERYSEGMENT4 */
  query.pInputData = &physical_adapter;
  query.InputDataSize = sizeof physical_adapter;
  query.pOutputData = &out;
  query.OutputDataSize = sizeof out;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), success);
  CHECK_EQ(out.NbSegment, 1u); }

TEST(adapter_query_adapter_info, declines_every_other_type_without_touching_either_buffer)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  const std::int32_t out_of_range[] = {-1, last_query_type + 1, 0x7FFFFFFF, static_cast<std::int32_t>(0x80000000u)};
  for (std::int32_t type = 0; type <= last_query_type + 4; type++) {
      const std::int32_t requested = type <= last_query_type ? type : out_of_range[type - last_query_type - 1];
      if (requested == query_driver_caps || requested == query_wddm_device_caps || requested == 11 ||
          (requested >= 13 && requested <= 16))
          continue;
      GuardedBuffer input(64u), output(64u);
      std::memset(input.data(), 0xEE, 64u);
      std::memset(output.data(), 0xEE, 64u);
      DxgkargQueryAdapterInfoRecord query = {};
      query.Type = requested;
      query.pInputData = input.data();
      query.InputDataSize = 64u;
      query.pOutputData = output.data();
      query.OutputDataSize = 64u;
      CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), not_implemented);
      bool untouched = true;
      for (std::uint32_t byte = 0u; byte < 64u; byte++)
          untouched = untouched && input.data()[byte] == 0xEEu && output.data()[byte] == 0xEEu;
      CHECK(untouched);
  } }

TEST(adapter_query_adapter_info, every_answer_is_a_status_dxgkrnl_accepts)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  for (std::int32_t type = -1; type <= last_query_type + 1; type++) {
      const std::uint32_t sizes[] = {0u, 3u, 4u, 0x24Fu, 0x250u};
      for (std::uint32_t size : sizes) {
          std::uint8_t input[0x250] = {}, output[0x250] = {};
          DxgkargQueryAdapterInfoRecord query = {};
          query.Type = type;
          query.pInputData = input;
          query.InputDataSize = size;
          query.pOutputData = output;
          query.OutputDataSize = size;
          CHECK(accepted_by_dxgkrnl(nt(fixture.adapter().query_adapter_info(&query))));
      }
  }
  CHECK(accepted_by_dxgkrnl(nt(fixture.adapter().query_adapter_info(nullptr)))); }

TEST(adapter_query_adapter_info, a_null_argument_block_or_short_driver_caps_is_invalid)
{ Fixture fixture; std::uint8_t output[0x250] = {};
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(nullptr)), invalid_parameter);
  DxgkargQueryAdapterInfoRecord query = {};
  query.Type = query_driver_caps;
  query.pOutputData = output;
  query.OutputDataSize = 0x24Fu;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter);
  query.pOutputData = nullptr;
  query.OutputDataSize = 0x250u;
  CHECK_EQ(nt(fixture.adapter().query_adapter_info(&query)), invalid_parameter); }

TEST(adapter_query_adapter_info, is_passive_only)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(query_driver_caps_of(fixture.adapter(), caps));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

/* ---- Escape ---------------------------------------------------------------- */

TEST(adapter_escape, no_escape_is_defined_so_every_escape_is_not_supported)
{ Fixture fixture; std::uint8_t data[64] = {};
  DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  const std::uint32_t sizes[] = {1u, 4u, 64u};
  for (std::uint32_t size : sizes) {
      escape.PrivateDriverDataSize = size;
      CHECK_EQ(nt(fixture.adapter().escape(&escape)), not_supported);
  } }

TEST(adapter_escape, missing_or_empty_private_data_is_invalid)
{ Fixture fixture; std::uint8_t data[4] = {};
  CHECK_EQ(nt(fixture.adapter().escape(nullptr)), invalid_parameter);
  DxgkargEscapeRecord escape = {};
  escape.PrivateDriverDataSize = 4u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter);
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = 0u;
  CHECK_EQ(nt(fixture.adapter().escape(&escape)), invalid_parameter); }

TEST(adapter_escape, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().escape(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- CollectDbgInfo -------------------------------------------------------- */

namespace {

std::uint32_t collect(Adapter &adapter, std::uint8_t *buffer, std::uint64_t size, DxgkargCollectDbgInfoExtRecord *ext)
{
    DxgkargCollectDbgInfoRecord record = {};
    record.pBuffer = buffer;
    record.BufferSize = size;
    record.pExtension = ext;
    return nt(adapter.collect_dbg_info(&record));
}

}  // namespace

TEST(adapter_collect_dbg_info, writes_the_started_and_powered_state)
{ Fixture fixture; GuardedBuffer buffer(av::DebugReport::report_bytes);
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(collect(fixture.adapter(), buffer.data(), av::DebugReport::report_bytes, nullptr), success);
  CHECK_EQ(buffer.data()[0], 0x52u);   /* 'R' */
  CHECK_EQ(buffer.data()[6], 1u);      /* started */
  CHECK_EQ(buffer.data()[7], 1u);      /* powered */
  CHECK(buffer.data()[8] != 0u || buffer.data()[12] != 0u);   /* VERSION_1 negotiated */
  CHECK(buffer.guards_intact()); }

TEST(adapter_collect_dbg_info, a_stopped_adapter_reports_stopped)
{ Fixture fixture; std::uint8_t buffer[av::DebugReport::report_bytes] = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK_EQ(collect(fixture.adapter(), buffer, sizeof buffer, nullptr), success);
  CHECK_EQ(buffer[6], 0u);
  CHECK_EQ(buffer[7], 0u);
  CHECK_EQ(buffer[8], 0u); }

TEST(adapter_collect_dbg_info, the_report_carries_the_node_state)
{ Fixture fixture; std::uint8_t buffer[av::DebugReport::report_bytes] = {};
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 4u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(collect(fixture.adapter(), buffer, sizeof buffer, nullptr), success);
  CHECK_EQ(buffer[16], 4u);   /* last submitted */
  CHECK_EQ(buffer[20], 0u);   /* last completed */
  CHECK_EQ(buffer[32], 1u);   /* pending */ }

TEST(adapter_collect_dbg_info, a_short_buffer_gets_whole_fields_only_and_nothing_past_it)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  for (std::uint64_t size = 0u; size < av::DebugReport::report_bytes; size++) {
      GuardedBuffer buffer(av::DebugReport::report_bytes);
      std::memset(buffer.data(), 0xEE, av::DebugReport::report_bytes);
      CHECK_EQ(collect(fixture.adapter(), buffer.data(), size, nullptr), success);
      CHECK_EQ(buffer.data()[av::DebugReport::report_bytes - 1u], 0xEEu);
      CHECK(buffer.guards_intact());
  } }

TEST(adapter_collect_dbg_info, zeroes_the_extension_and_rejects_a_missing_buffer)
{ Fixture fixture; DxgkargCollectDbgInfoExtRecord ext;
  std::memset(&ext, 0xEE, sizeof ext);
  CHECK_EQ(collect(fixture.adapter(), nullptr, 0u, &ext), success);
  CHECK_EQ(ext.BucketingKey, 0u);
  CHECK_EQ(ext.Reserved7, 0u);
  CHECK_EQ(collect(fixture.adapter(), nullptr, 16u, nullptr), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().collect_dbg_info(nullptr)), invalid_parameter); }

TEST(adapter_collect_dbg_info, allocates_nothing)
{ Fixture fixture; std::uint8_t buffer[64] = {};
  CHECK_EQ(fixture.start(), success);
  const std::uint32_t allocations = fixture.platform().allocations_made();
  CHECK_EQ(collect(fixture.adapter(), buffer, sizeof buffer, nullptr), success);
  CHECK_EQ(fixture.platform().allocations_made(), allocations);
  CHECK_EQ(fixture.platform().locks_held(), 0u); }

TEST(adapter_collect_dbg_info, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().collect_dbg_info(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

/* ---- Devices and allocations (feature 008) ---------------------------------- */

namespace {

/* The runtime's shadow surface, created the way dxgkrnl creates one: the
 * driver describes it, then allocates it. */
class ShadowSurface {
public:
    explicit ShadowSurface(Adapter &adapter) : adapter_(adapter), surface_{64u, 32u, 21, 0u}, descriptor_{}, info_{}
    {
        av::DxgkargGetStandardAllocationDriverDataRecord describe = {};
        describe.StandardAllocationType = 2;   /* D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE */
        describe.pCreateSurfaceData = &surface_;
        describe.pAllocationPrivateDriverData = descriptor_;
        described_ = nt(adapter_.get_standard_allocation_driver_data(&describe));
        info_.pPrivateDriverData = descriptor_;
        info_.PrivateDriverDataSize = describe.AllocationPrivateDriverDataSize;
    }

    std::uint32_t described() const { return described_; }

    std::uint32_t create()
    {
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info_;
        return nt(adapter_.create_allocation(&create));
    }

    std::uint32_t destroy()
    {
        void *handle = info_.hAllocation;
        av::DxgkargDestroyAllocationRecord destroy = {};
        destroy.NumAllocations = 1u;
        destroy.pAllocationList = &handle;
        return nt(adapter_.destroy_allocation(&destroy));
    }

    void *handle() const { return info_.hAllocation; }
    std::uint32_t pitch() const { return surface_.Pitch; }

private:
    Adapter                             &adapter_;
    av::D3dkmdtShadowSurfaceDataRecord   surface_;
    std::uint8_t                         descriptor_[av::AllocationDescriptor::encoded_size];
    av::DxgkAllocationInfoRecord         info_;
    std::uint32_t                        described_ = 0u;
};

}  // namespace

TEST(adapter_create_device, answers_a_device_that_leads_back_to_the_adapter)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord request = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&request)), success);
  CHECK(&static_cast<av::Device *>(request.hDevice)->adapter() == &fixture.adapter());
  CHECK_EQ(nt(fixture.adapter().destroy_device(request.hDevice)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(request.hDevice)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().create_device(nullptr)), invalid_parameter); }

TEST(adapter_create_allocation, a_standard_surface_lives_through_open_describe_close_and_destroy)
{ Fixture fixture; av::FakeHandleTable handles;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.described(), success);
  CHECK_EQ(shadow.pitch(), 256u);
  CHECK_EQ(shadow.create(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkOpenAllocationInfoRecord open_info = {};
  open_info.hAllocation = handles.insert(shadow.handle());
  av::DxgkargOpenAllocationRecord open = {};
  open.NumAllocations = 1u;
  open.pOpenAllocation = &open_info;
  CHECK_EQ(nt(fixture.adapter().open_allocation(device.hDevice, &open)), success);
  CHECK_EQ(open.Pitch, 256u);
  av::DxgkargDescribeAllocationRecord describe = {};
  describe.hAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().describe_allocation(&describe)), success);
  CHECK_EQ(describe.Width, 64u);
  CHECK_EQ(shadow.destroy(), invalid_device_state);   /* still open */
  void *open_handle = open_info.hDeviceSpecificAllocation;
  av::DxgkargCloseAllocationRecord close = {};
  close.NumAllocations = 1u;
  close.pOpenHandleList = &open_handle;
  CHECK_EQ(nt(fixture.adapter().close_allocation(device.hDevice, &close)), success);
  CHECK_EQ(shadow.destroy(), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  CHECK(fixture.adapter().invariants_hold()); }

TEST(adapter_create_allocation, every_allocation_ddi_refuses_a_missing_argument_block)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().create_allocation(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_allocation(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().describe_allocation(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().get_standard_allocation_driver_data(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().open_allocation(nullptr, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().close_allocation(nullptr, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(nullptr)), invalid_parameter); }

TEST(adapter_create_allocation, before_start_there_is_no_segment_to_place_it_in)
{ Fixture fixture;
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.create(), invalid_parameter); }

/* A bring-up that never reaches a committable VidPn is read back from the log,
 * so each step of the display path says what it answered (FR-003). The lines
 * are asserted in the order dxgkrnl walks them. */
TEST(adapter, each_step_of_the_display_path_says_what_it_answered)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: GetStandardAllocationDriverData status 0x00000000") == 0);
  CHECK_EQ(shadow.create(), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: CreateAllocation status 0x00000000") == 0);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: CreateDevice status 0x00000000") == 0);
  av::DxgkargDescribeAllocationRecord describe = {};
  describe.hAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().describe_allocation(&describe)), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: DescribeAllocation status 0x00000000") == 0);
  av::DxgkargSetDisplayPrivateDriverFormatRecord format = {};
  format.PrimaryAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&format)), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: SetDisplayPrivateDriverFormat status 0x00000000") == 0);
  av::DxgkargSetVidPnSourceVisibilityRecord visibility = {};
  CHECK_EQ(nt(fixture.adapter().set_vidpn_source_visibility(&visibility)), success);
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "rvkgpu: SetVidPnSourceVisibility status 0x00000000") == 0);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success);
  CHECK_EQ(shadow.destroy(), success);
  CHECK_EQ(nt(fixture.adapter().stop_device()), success);
  CHECK(std::strcmp(fixture.platform().last_log(), "rvkgpu: StopDevice status 0x00000000") == 0); }

TEST(adapter_set_display_private_driver_format, takes_no_primary_or_one_of_ours)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  ShadowSurface shadow(fixture.adapter());
  CHECK_EQ(shadow.create(), success);
  av::DxgkargSetDisplayPrivateDriverFormatRecord request = {};
  request.PrivateDriverFormatAttribute = 5u;
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), success);
  request.PrimaryAllocation = shadow.handle();
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), success);
  request.PrimaryAllocation = &request;
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), invalid_parameter);
  request.PrimaryAllocation = nullptr;
  request.VidPnSourceId = 1u;
  CHECK_EQ(nt(fixture.adapter().set_display_private_driver_format(&request)), invalid_parameter);
  CHECK_EQ(shadow.destroy(), success); }

TEST(adapter, removal_with_devices_and_allocations_outstanding_releases_them)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  { FakeDxgkrnlAdapter dxgkrnl(platform, function);
    Adapter adapter(platform, &function);
    std::uint32_t sources = 0u, children = 0u;
    CHECK_EQ(nt(adapter.start_device(nullptr, &dxgkrnl.record(), &sources, &children)), success);
    ShadowSurface shadow(adapter);
    CHECK_EQ(shadow.create(), success);
    av::DxgkargCreateDeviceRecord device = {};
    CHECK_EQ(nt(adapter.create_device(&device)), success);
    CHECK_EQ(nt(adapter.stop_device()), success); }
  CHECK_EQ(platform.live_allocations(), 0u); }

/* ---- Contexts (feature 009) --------------------------------------------------- */

TEST(adapter_create_context, answers_a_context_on_a_device_and_destroys_it)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  CHECK(&static_cast<av::Context *>(context.hContext)->device().adapter() == &fixture.adapter());
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_render, needs_a_live_context_then_renders)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint8_t command[16] = {1, 2, 3}, dma[16] = {};
  av::DxgkargRenderRecord render = {};
  render.pCommand = command;
  render.CommandLength = sizeof command;
  render.pDmaBuffer = dma;
  render.DmaSize = sizeof dma;
  CHECK_EQ(nt(fixture.adapter().render(&fixture, &render)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().render(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().render(context.hContext, &render)), success);
  CHECK_EQ(dma[2], 3u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_patch, patches_or_refuses_a_missing_block)
{ Fixture fixture;
  std::uint8_t dma[8] = {};
  av::DxgkargPatchRecord patch = {};
  patch.pDmaBuffer = dma;
  patch.DmaBufferSize = sizeof dma;
  CHECK_EQ(nt(fixture.adapter().patch(&patch)), success);
  CHECK_EQ(nt(fixture.adapter().patch(nullptr)), invalid_parameter); }

/* ---- Submission (feature 009) ------------------------------------------------- */

TEST(adapter_submit_command_virtual, queues_a_context_s_buffer_inside_its_root)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x4u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  av::DxgkargSubmitCommandVirtualRecord submit = {};
  submit.hContext = context.hContext;
  submit.DmaBufferVirtualAddress = 0x8000000000ull;
  submit.SubmissionFenceId = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u);
  const av::DxgkargSetRootPageTableRecord root = {context.hContext, 2u, 0u, 0x4000u, 1u, 0u};
  fixture.adapter().set_root_page_table(&root);
  submit.SubmissionFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  submit.DmaBufferVirtualAddress = 0x10000u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), success);
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);   /* fence not newer */
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 2u);
  submit.SubmissionFenceId = 3u;
  submit.NodeOrdinal = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  submit.NodeOrdinal = 0u;
  submit.hContext = &fixture;
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(&submit)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().submit_command_virtual(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  av::DxgkargQueryCurrentFenceRecord fence = {};
  CHECK_EQ(nt(fixture.adapter().query_current_fence(&fence)), success);
  CHECK_EQ(fence.CurrentFence, 2u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_set_root_page_table, a_root_for_no_context_or_no_table_is_logged)
{ Fixture fixture;
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  const std::uint32_t logged = fixture.platform().logged();
  const av::DxgkargSetRootPageTableRecord good = {context.hContext, 2u, 0u, 0x4000u, 512u, 0u};
  fixture.adapter().set_root_page_table(&good);
  CHECK_EQ(fixture.platform().logged(), logged);
  const av::DxgkargSetRootPageTableRecord empty = {context.hContext, 2u, 0u, 0x4000u, 0u, 0u};
  fixture.adapter().set_root_page_table(&empty);
  const av::DxgkargSetRootPageTableRecord stranger = {&fixture, 2u, 0u, 0x4000u, 1u, 0u};
  fixture.adapter().set_root_page_table(&stranger);
  fixture.adapter().set_root_page_table(nullptr);
  CHECK_EQ(fixture.platform().logged(), logged + 3u);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_get_root_page_table_size, is_eight_bytes_an_entry)
{ Fixture fixture;
  av::DxgkargGetRootPageTableSizeRecord request = {512u, 0u};
  CHECK_EQ(fixture.adapter().get_root_page_table_size(&request), 4096u);
  CHECK_EQ(fixture.adapter().get_root_page_table_size(nullptr), 0u); }

TEST(adapter_create_process, keeps_no_driver_process)
{ Fixture fixture;
  av::DxgkargCreateProcessRecord request = {};
  request.hKmdProcess = &fixture;
  CHECK_EQ(nt(fixture.adapter().create_process(&request)), success);
  CHECK(request.hKmdProcess == nullptr);
  CHECK_EQ(nt(fixture.adapter().create_process(nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_process(nullptr)), success); }

TEST(adapter_wddm_2_0_process_and_root_ddis, are_passive_only)
{ Fixture fixture;
  av::DxgkargGetRootPageTableSizeRecord size = {1u, 0u};
  const av::DxgkargSetStablePowerStateRecord stable = {1};
  fixture.adapter().set_stable_power_state(&stable);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().submit_command_virtual(nullptr));
  fixture.adapter().set_root_page_table(nullptr);
  static_cast<void>(fixture.adapter().get_root_page_table_size(&size));
  static_cast<void>(fixture.adapter().create_process(nullptr));
  static_cast<void>(fixture.adapter().destroy_process(nullptr));
  fixture.adapter().set_stable_power_state(&stable);
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(adapter_control_mode_behavior, answers_before_start_with_nothing_satisfied)
{ Fixture fixture;
  av::DxgkargControlModeBehaviorRecord request = {0x1u, 0u, 0u};
  CHECK_EQ(nt(fixture.adapter().control_mode_behavior(&request)), success);
  CHECK_EQ(request.NotSatisfied, 0x1u);
  CHECK_EQ(nt(fixture.adapter().control_mode_behavior(nullptr)), invalid_parameter); }

TEST(adapter_update_monitor_link_info, describes_the_monitor_on_the_one_target)
{ Fixture fixture;
  av::DxgkargUpdateMonitorLinkInfoRecord link = {0u, 0x1u, 0xEEu, 0xEEu};
  CHECK_EQ(nt(fixture.adapter().update_monitor_link_info(&link)), success);
  CHECK_EQ(link.MonitorLinkInfoCapabilities, 0u);
  CHECK_EQ(link.MonitorLinkInfoUsageHints, 0x1u);
  link.VideoPresentTargetId = 1u;
  CHECK_EQ(nt(fixture.adapter().update_monitor_link_info(&link)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().update_monitor_link_info(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().update_monitor_link_info(nullptr));
  static_cast<void>(fixture.adapter().control_mode_behavior(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(adapter_hw_queue, submissions_complete_through_the_dpc_and_a_reset_drops_them)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargCreateDeviceRecord device = {};
  CHECK_EQ(nt(fixture.adapter().create_device(&device)), success);
  av::DxgkargCreateContextRecord context = {};
  context.Flags = 0x14u;
  CHECK_EQ(nt(fixture.adapter().create_context(device.hDevice, &context)), success);
  std::uint64_t fence = 0u;
  av::DxgkargCreateHwQueueRecord queue = {};
  queue.HwQueueProgressFenceCPUVirtualAddress = &fence;
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, &queue)), success);
  CHECK_EQ(nt(fixture.adapter().create_hw_queue(context.hContext, nullptr)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), invalid_parameter);   /* a queue still runs on it */
  av::DxgkargSubmitCommandToHwQueueRecord submit = {};
  submit.hHwQueue = queue.hHwQueue;
  submit.HwQueueProgressFenceId = 1u;
  submit.HwQueueProgressFenceCpuVa = &fence;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  submit.HwQueueProgressFenceId = 2u;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 2u);
  submit.DmaBufferPrivateDataSize = 4u;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), invalid_parameter);
  submit.DmaBufferPrivateDataSize = 0u;
  submit.HwQueueProgressFenceCpuVa = nullptr;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(nullptr)), invalid_parameter);
  const std::uint32_t reported = fixture.dxgkrnl().interrupts();
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fence, 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), reported + 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(reported).InterruptType, 11);
  submit.HwQueueProgressFenceId = 3u;
  submit.HwQueueProgressFenceCpuVa = &fence;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  const av::DxgkargResetHwEngineRecord reset = {0u, 0u};
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(&reset)), success);
  const av::DxgkargResetHwEngineRecord other_node = {1u, 0u};
  const av::DxgkargResetHwEngineRecord other_engine = {0u, 1u};
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(&other_node)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(&other_engine)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().reset_hw_engine(nullptr)), invalid_parameter);
  submit.HwQueueProgressFenceId = 4u;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(queue.hHwQueue)), success);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fence, 2u);   /* neither the reset's nor the destroyed queue's work is written */
  CHECK_EQ(nt(fixture.adapter().destroy_hw_queue(queue.hHwQueue)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().destroy_context(context.hContext)), success);
  CHECK_EQ(nt(fixture.adapter().destroy_device(device.hDevice)), success); }

TEST(adapter_resume_hw_engine, resumes_the_one_engine)
{ Fixture fixture;
  const av::DxgkargResumeHwEngineRecord engine = {0u, 0u};
  const av::DxgkargResumeHwEngineRecord other_node = {1u, 0u};
  const av::DxgkargResumeHwEngineRecord other_engine = {0u, 1u};
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(&engine)), success);
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(&other_node)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(&other_engine)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().resume_hw_engine(nullptr)), invalid_parameter);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().resume_hw_engine(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

TEST(adapter_dpc_routine, a_refused_progress_fence_signal_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint64_t fence = 0u;
  av::DxgkargSubmitCommandToHwQueueRecord submit = {};
  submit.hHwQueue = &fixture;
  submit.HwQueueProgressFenceId = 1u;
  submit.HwQueueProgressFenceCpuVa = &fence;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&submit)), success);
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  const std::uint32_t logged = fixture.platform().logged();
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), logged + 1u);
  const av::DxgkargResetHwEngineRecord reset = {0u, 0u};
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.adapter().reset_hw_engine(&reset));
  static_cast<void>(fixture.adapter().create_hw_queue(nullptr, nullptr));
  static_cast<void>(fixture.adapter().destroy_hw_queue(nullptr));
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.adapter().submit_command_to_hw_queue(nullptr));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {} }

TEST(adapter_submit_command, queues_and_asks_for_the_dpc_that_completes_it)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  CHECK_EQ(fixture.dxgkrnl().dpcs_queued(), 1u);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_parameter);
  submit.SubmissionFenceId = 2u;
  submit.NodeOrdinal = 1u;
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().submit_command(nullptr)), invalid_parameter);
  av::DxgkargPreemptCommandRecord preempt = {};
  preempt.PreemptionFenceId = 5u;
  CHECK_EQ(nt(fixture.adapter().preempt_command(&preempt)), success);
  preempt.NodeOrdinal = 3u;
  CHECK_EQ(nt(fixture.adapter().preempt_command(&preempt)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().preempt_command(nullptr)), invalid_parameter);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 1u);
  av::DxgkargQueryCurrentFenceRecord fence = {};
  CHECK_EQ(nt(fixture.adapter().query_current_fence(&fence)), success);
  CHECK_EQ(fence.CurrentFence, 1u);
  fence.NodeOrdinal = 1u;
  CHECK_EQ(nt(fixture.adapter().query_current_fence(&fence)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().query_current_fence(nullptr)), invalid_parameter); }

/* A fault storm at DISPATCH would otherwise turn one broken device into an
 * unbounded stream of log lines, so every refusal line in the DPC and every DDI
 * status line is spent from the budget (feature 037, FR-003). Once the window is
 * spent, the same four refusals and the same DDI say nothing at all. */
TEST(adapter_dpc_routine, a_storm_of_refusals_is_silent_once_the_window_is_spent)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  std::uint64_t progress = 0u;
  av::DxgkargSubmitCommandToHwQueueRecord hw_submit = {};
  hw_submit.hHwQueue = &fixture;
  hw_submit.HwQueueProgressFenceId = 1u;
  hw_submit.HwQueueProgressFenceCpuVa = &progress;
  CHECK_EQ(nt(fixture.adapter().submit_command_to_hw_queue(&hw_submit)), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  const av::DxgkargRecommendVidPnTopologyRecord topology = {};
  for (std::uint32_t line = 0u; line <= av::LogBudget::lines_per_window; line++)
      CHECK_EQ(nt(fixture.adapter().recommend_vidpn_topology(&topology)), no_recommended_vidpn_topology);
  const std::uint32_t spent = fixture.platform().logged();
  CHECK_EQ(nt(fixture.adapter().recommend_vidpn_topology(&topology)), no_recommended_vidpn_topology);
  CHECK_EQ(fixture.platform().logged(), spent);
  DxgkDriverCapsRecord caps = {};
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK_EQ(fixture.platform().logged(), spent);   /* the QueryAdapterInfo line too */
  for (std::uint16_t index = 0u; index < 2u; index++) {
      const av::FakeCommonConfig::Queue &queue = fixture.function().common().queue(index);
      const auto used = static_cast<std::uint8_t *>(fixture.platform().translate_physical(queue.device, 4u));
      used[2] = 1u;   /* used.idx: a completion nothing was submitted for */
  }
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  /* The one line the DPC still writes is the device failing itself, which it
   * does once and then stops running - not a storm to budget. */
  CHECK(std::strcmp(fixture.platform().last_log(),
                    "virtio-gpu: a queue returned a request it was never given") == 0);
  CHECK_EQ(fixture.platform().logged(), spent + 1u); }

TEST(adapter_dpc_routine, a_refused_completion_is_logged)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  const std::uint32_t logged = fixture.platform().logged();
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().logged() > logged); }

TEST(adapter_build_paging_buffer, answers_aperture_operations_only)
{ Fixture fixture;
  av::DxgkargBuildPagingBufferRecord request = {};
  request.Operation = 5;
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&request)), success);
  request.Operation = 1;
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(&request)), invalid_parameter);
  CHECK_EQ(nt(fixture.adapter().build_paging_buffer(nullptr)), invalid_parameter); }

/* ---- TDR (feature 010) ---------------------------------------------------------- */

TEST(adapter_reset_from_timeout, abandons_packets_resets_the_device_then_restart_brings_both_back)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  CHECK(fixture.adapter().invariants_hold());
  CHECK_EQ(fixture.function().common().status(), 0u);
  submit.SubmissionFenceId = 2u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_device_state);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  CHECK_EQ(fixture.function().common().status(), driver_ok_status);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), success);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).FenceId, 2u); }

TEST(adapter_reset_from_timeout, before_start_touches_no_device)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), success);
  CHECK_EQ(fixture.function().common().status_writes(), 0u); }

TEST(adapter_restart_from_timeout, a_device_that_will_not_come_back_is_reported_and_submissions_stay_refused)
{ Fixture fixture;
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(nt(fixture.adapter().reset_from_timeout()), success);
  fixture.function().common().set_queues(1u, 256u);
  CHECK_EQ(nt(fixture.adapter().restart_from_timeout()), not_found);
  av::DxgkargSubmitCommandRecord submit = {};
  submit.SubmissionFenceId = 1u;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_EQ(nt(fixture.adapter().submit_command(&submit)), invalid_device_state);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().live_allocations(), 1u);   /* the cursor's backing, held while started */ }
