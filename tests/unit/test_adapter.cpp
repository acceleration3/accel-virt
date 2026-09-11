#include "../../src/core/adapter.hpp"
#include "../../src/core/ddi/dxgk_child_descriptor_record.hpp"
#include "../../src/core/ddi/dxgk_child_status_record.hpp"
#include "../../src/core/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "../../src/core/ddi/dxgk_allocation_info_record.hpp"
#include "../../src/core/ddi/dxgk_open_allocation_info_record.hpp"
#include "../../src/core/ddi/dxgkarg_close_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_context_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_device_record.hpp"
#include "../../src/core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "../../src/core/ddi/dxgkarg_open_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "../../src/core/ddi/dxgkarg_patch_record.hpp"
#include "../../src/core/ddi/dxgkarg_preempt_command_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_current_fence_record.hpp"
#include "../../src/core/ddi/dxgkarg_submit_command_record.hpp"
#include "../../src/core/ddi/dxgkarg_render_record.hpp"
#include "../../src/core/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../../src/core/ddi/dxgk_query_segment_out4_record.hpp"
#include "../../src/core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "../../src/core/ddi/dxgkarg_escape_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "../../src/core/ddi/linked_device_record.hpp"
#include "../../src/core/context.hpp"
#include "../../src/core/debug_report.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/fake_dxgkrnl_adapter.hpp"
#include "../harness/fake_handle_table.hpp"
#include "../harness/fake_virtio_gpu_function.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

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
constexpr std::uint32_t child_descriptor_not_supported = 0xC01E0401u;
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
  CHECK_EQ(fixture.platform().live_allocations(), 0u); }

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
  fixture.platform().set_irql(Irql::dispatch);
  fixture.adapter().dpc_routine();
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().logged(), 0u);
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

TEST(adapter_query_device_descriptor, answers_as_basicdisplay_does_without_an_edid)
{ Fixture fixture;
  CHECK_EQ(nt(fixture.adapter().query_device_descriptor(0u, nullptr)), child_descriptor_not_supported); }

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

TEST(adapter_query_adapter_info, answers_driver_caps_with_wddm_1_0)
{ Fixture fixture; DxgkDriverCapsRecord caps = {};
  CHECK_EQ(fixture.start(), success);
  CHECK_EQ(query_driver_caps_of(fixture.adapter(), caps), success);
  CHECK_EQ(caps.WDDMVersion, 0x1000); }

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
  CHECK_EQ(caps, 0x1000); }

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
      if (requested == query_driver_caps || requested == query_wddm_device_caps || requested == 11) continue;
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
