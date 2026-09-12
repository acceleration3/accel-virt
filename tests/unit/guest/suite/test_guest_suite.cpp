/* The suite end to end on MockKmt: a driver that does everything right, one
 * that refuses at each call, one that answers garbage, and a guest without the
 * adapter. The scenarios' own tests are this file's: each scenario is only
 * reachable with an AdapterSearch the suite shares. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/allocation_scenario.hpp"
#include "guest/suite/gpu_mapping_scenario.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_suite.hpp"
#include "guest/suite/submission_scenario.hpp"
#include "guest/suite/vsync_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AllocationScenario;
using av::GpuMappingScenario;
using av::GuestReport;
using av::GuestSuite;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubmissionScenario;
using av::VsyncScenario;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

}  // namespace

TEST(guest_suite, a_driver_that_does_everything_right_passes_every_step)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1414u, 0x008Cu, 0u, 0u), 3100u);
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  GuestReport report;
  GuestSuite suite(kmt, report);
  CHECK_EQ(suite.run(), 0u);
  CHECK(std::strcmp(report.text(),
                    "ok adapter.find\n"
                    "ok adapter.driver_version\n"
                    "ok allocation.adapter\n"
                    "ok allocation.device\n"
                    "ok allocation.create\n"
                    "ok allocation.refuse_hostile\n"
                    "ok allocation.teardown\n"
                    "ok mapping.adapter\n"
                    "ok mapping.device\n"
                    "ok mapping.allocation\n"
                    "ok mapping.paging_queue\n"
                    "ok mapping.map\n"
                    "ok mapping.teardown\n"
                    "ok submission.adapter\n"
                    "ok submission.device\n"
                    "ok submission.allocation\n"
                    "ok submission.paging_queue\n"
                    "ok submission.map\n"
                    "ok submission.context\n"
                    "ok submission.submit\n"
                    "ok submission.wait\n"
                    "ok submission.teardown\n"
                    "ok vsync.adapter\n"
                    "ok vsync.device\n"
                    "ok vsync.blanks\n"
                    "ok vsync.rate\n"
                    "ok vsync.caught\n"
                    "ok vsync.teardown\n"
                    "ok suite.close_adapter\n"
                    "rvktest: 29 passed, 0 failed\n") == 0);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK(report.invariants_hold()); }

TEST(guest_suite, without_the_adapter_every_scenario_fails_without_calling_d3dkmt)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u), 1300u);
  GuestReport report;
  GuestSuite suite(kmt, report);
  CHECK_EQ(suite.run(), 5u);
  CHECK(has_line(report, "FAIL adapter.find status=3"));
  CHECK(has_line(report, "FAIL allocation.adapter status=3"));
  CHECK(has_line(report, "FAIL mapping.adapter status=3"));
  CHECK(has_line(report, "FAIL submission.adapter status=3"));
  CHECK(has_line(report, "FAIL vsync.adapter status=3"));
  CHECK(has_line(report, "ok suite.close_adapter"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(guest_suite, an_adapter_of_another_wddm_version_or_that_will_not_say_fails_that_step)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3100u);
  GuestReport report;
  AdapterSearch search(kmt);
  AdapterScenario scenario(kmt, report, search);
  scenario.run();
  CHECK(has_line(report, "ok adapter.find"));
  CHECK(has_line(report, "FAIL adapter.driver_version status=6"));
  CHECK_STATUS(search.close(), Status::ok);
  kmt.fail_at(MockKmt::Call::query_driver_version, 1u, 0xC0000001u);
  scenario.run();
  CHECK(has_line(report, "FAIL adapter.driver_version status=7 code=0xC0000001"));
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(guest_suite, a_refusal_at_any_call_is_reported_and_leaves_nothing_open)
{ const MockKmt::Call calls[] = {
      MockKmt::Call::create_device, MockKmt::Call::create_allocation, MockKmt::Call::create_paging_queue,
      MockKmt::Call::make_resident, MockKmt::Call::map_gpu_virtual_address, MockKmt::Call::create_context,
      MockKmt::Call::submit, MockKmt::Call::wait_for_context, MockKmt::Call::destroy_context,
      MockKmt::Call::close_adapter};
  for (MockKmt::Call call : calls) {
      MockKmt kmt;
      kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
      kmt.fail_at(call, 1u, 0xC0000001u);
      GuestReport report;
      { GuestSuite suite(kmt, report);
        CHECK(suite.run() != 0u);
        CHECK(has_line(report, "code=0xC0000001")); }
      CHECK(report.invariants_hold());
  } }

TEST(guest_suite, each_scenario_stops_at_its_first_failure_and_still_tears_down)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  AllocationScenario allocation(kmt, report, search);
  GpuMappingScenario mapping(kmt, report, search);
  SubmissionScenario submission(kmt, report, search);
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  allocation.run();
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  mapping.run();
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  submission.run();
  CHECK(!has_line(report, "allocation.create"));
  CHECK(!has_line(report, "mapping.allocation"));
  CHECK(!has_line(report, "submission.allocation"));
  CHECK(has_line(report, "ok allocation.teardown"));
  CHECK(has_line(report, "ok mapping.teardown"));
  CHECK(has_line(report, "ok submission.teardown"));
  kmt.fail_at(MockKmt::Call::create_paging_queue, 1u, 0xC0000001u);
  mapping.run();
  kmt.fail_at(MockKmt::Call::make_resident, 1u, 0xC0000001u);
  submission.run();
  kmt.fail_at(MockKmt::Call::create_context, 1u, 0xC0000001u);
  submission.run();
  kmt.fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
  submission.run();
  kmt.fail_at(MockKmt::Call::create_allocation, 1u, 0xC0000001u);
  mapping.run();
  kmt.fail_at(MockKmt::Call::create_allocation, 1u, 0xC0000001u);
  submission.run();
  kmt.fail_at(MockKmt::Call::create_paging_queue, 1u, 0xC0000001u);
  submission.run();
  kmt.fail_at(MockKmt::Call::make_resident, 1u, 0xC0000001u);
  mapping.run();
  CHECK_EQ(kmt.open_objects(), 1u);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK_STATUS(search.close(), Status::ok); }

/* ---- Vertical blanks (feature 038, US3) ------------------------------------ */

TEST(vsync_scenario, counts_the_blanks_of_a_window_and_judges_the_rate)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  VsyncScenario vsync(kmt, report, search);
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK(!vsync.paced());
  vsync.run();
  CHECK(has_line(report, "ok vsync.device"));
  CHECK(has_line(report, "ok vsync.blanks"));
  CHECK(has_line(report, "ok vsync.rate"));
  CHECK(has_line(report, "ok vsync.teardown"));
  CHECK_EQ(vsync.rate_centihertz(), VsyncScenario::expected_hz * 100u);
  /* Every interval the mock gives is the same, so the rate a pulse-woken loop
   * caught is the driver's. */
  CHECK_EQ(vsync.caught_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK(vsync.paced());
  /* The blank that opened the window is not one of the counted ones. */
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank), VsyncScenario::counted_blanks + 1u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* Blanks slower than every other frame, or faster than every frame, are not
 * the committed mode's: DWM's frame timing is built on them. */
TEST(vsync_scenario, an_interval_outside_one_frame_to_two_fails_the_step)
{ const std::uint64_t rates[] = {25u, 120u, 50u, 63u};
  for (const std::uint64_t hertz : rates) {
      MockKmt kmt;
      kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
      kmt.set_blank_period(MockKmt::ticks_per_second / hertz);
      AdapterSearch search(kmt);
      CHECK_STATUS(search.find(rvk), Status::ok);
      GuestReport report;
      VsyncScenario vsync(kmt, report, search);
      vsync.run();
      const bool within = hertz == 50u || hertz == 63u;
      CHECK_EQ(has_line(report, "ok vsync.rate"), within);
      CHECK_EQ(vsync.paced(), within);
      CHECK_STATUS(search.close(), Status::ok);
  } }

TEST(vsync_scenario, a_refused_or_timed_out_wait_stops_the_count)
{ const std::uint32_t codes[] = {0xC0000001u, 0xC00000B5u};   /* unsuccessful, IO timeout */
  for (const std::uint32_t code : codes) {
      MockKmt kmt;
      kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
      kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 5u, code);
      AdapterSearch search(kmt);
      CHECK_STATUS(search.find(rvk), Status::ok);
      GuestReport report;
      VsyncScenario vsync(kmt, report, search);
      vsync.run();
      CHECK(has_line(report, "FAIL vsync.blanks"));
      CHECK(!has_line(report, "vsync.rate"));
      CHECK(has_line(report, "ok vsync.teardown"));
      CHECK_STATUS(search.close(), Status::ok);
      CHECK_EQ(kmt.open_objects(), 0u);
  } }

TEST(vsync_scenario, without_a_device_it_counts_nothing)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK(has_line(report, "FAIL vsync.device"));
  CHECK(!has_line(report, "vsync.blanks"));
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank), 0u);
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK_STATUS(search.close(), Status::ok); }

/* A guest without the adapter never waits on its vertical blanks. */
TEST(vsync_scenario, without_the_adapter_it_calls_nothing)
{ MockKmt kmt;
  GuestReport report;
  AdapterSearch search(kmt);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK(has_line(report, "FAIL vsync.adapter"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }

/* A clock that does not move cannot say what the rate was. */
TEST(vsync_scenario, a_clock_that_stood_still_is_no_rate)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  kmt.set_blank_period(0u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK(has_line(report, "FAIL vsync.rate"));
  CHECK_STATUS(search.close(), Status::ok); }

/* A loop that misses pulses counts fewer blanks than the driver reported; the
 * shortest interval is still one the driver kept. */
TEST(vsync_scenario, judges_the_shortest_interval_not_the_average)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  /* Every fourth blank comes a frame late, as a missed pulse looks. */
  kmt.slip_every(4u, MockKmt::ticks_per_second / VsyncScenario::expected_hz);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK_EQ(vsync.rate_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK(vsync.paced());
  CHECK(vsync.caught_centihertz() < VsyncScenario::expected_hz * 100u);
  CHECK(has_line(report, "ok vsync.rate"));
  CHECK_STATUS(search.close(), Status::ok); }

/* The first blank opens the window; a driver that refuses it counts none. */
TEST(vsync_scenario, a_refusal_at_the_first_blank_counts_none)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 1u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK(has_line(report, "FAIL vsync.blanks"));
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK_EQ(vsync.caught_centihertz(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank), 1u);
  CHECK_STATUS(search.close(), Status::ok); }

/* A blank that came late does not become the driver's period: the shortest
 * interval after it does. */
TEST(vsync_scenario, a_later_interval_shorter_than_the_first_is_the_one_judged)
{ MockKmt kmt;
  kmt.add_adapter(rvk, AdapterScenario::wddm_3_2);
  kmt.slip_every(2u, MockKmt::ticks_per_second / VsyncScenario::expected_hz);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::ok);
  GuestReport report;
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK_EQ(vsync.rate_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK(vsync.paced());
  CHECK_STATUS(search.close(), Status::ok); }
