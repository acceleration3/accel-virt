#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/allocation_scenario.hpp"
#include "guest/suite/gpu_mapping_scenario.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_suite.hpp"
#include "guest/suite/host_visible_import_scenario.hpp"
#include "guest/suite/submission_scenario.hpp"
#include "guest/suite/timeout_scenario.hpp"
#include "guest/suite/transport_scenario.hpp"
#include "guest/suite/fence_stress_scenario.hpp"
#include "guest/suite/vsync_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"

#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::AllocationScenario;
using av::FenceStressScenario;
using av::GpuMappingScenario;
using av::GuestReport;
using av::GuestSuite;
using av::HostVisibleImportScenario;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubmissionScenario;
using av::TimeoutScenario;
using av::TransportScenario;
using av::VsyncScenario;

namespace {
constexpr std::uint32_t device_removed = 0xC00E0102u;

const PciIdentity av_gpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

}

TEST(guest_suite, a_driver_that_does_everything_right_passes_every_step)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1414u, 0x008Cu, 0u, 0u), 3100u);
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.answer_waits(nullptr, 0u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
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
                    "ok vsync.rate 6000\n"
                    "ok vsync.caught 6000\n"
                    "ok vsync.teardown\n"
                    "ok transport.adapter\n"
                    "ok transport.connection\n"
                    "ok transport.initialize\n"
                    "ok transport.nop 0\n"
                    "ok transport.latency 1000\n"
                    "ok transport.warm_nop 31\n"
                    "ok transport.warm_median 1000\n"
                    "ok transport.warm_fastest 1000\n"
                    "ok suite.close_adapter\n"
                    "avtest: 37 passed, 0 failed\n") == 0);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK(report.invariants_hold()); }

TEST(guest_suite, the_host_loss_run_stops_at_a_host_that_draws_nothing)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.answer_waits(nullptr, 0u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  CHECK_EQ(suite.run_host_loss(), 1u);
  CHECK(has_line(report, "ok host_loss.initialize\n"));
  CHECK(has_line(report, "FAIL host_loss.drawn"));
  CHECK(!has_line(report, "host_loss.lose"));
  CHECK(has_line(report, "ok suite.close_adapter\n"));
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(guest_suite, without_the_adapter_every_scenario_fails_without_calling_d3dkmt)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u), 1300u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  CHECK_EQ(suite.run(), 6u);
  CHECK(has_line(report, "FAIL adapter.find status=3"));
  CHECK(has_line(report, "FAIL allocation.adapter status=3"));
  CHECK(has_line(report, "FAIL mapping.adapter status=3"));
  CHECK(has_line(report, "FAIL submission.adapter status=3"));
  CHECK(has_line(report, "FAIL vsync.adapter status=3"));
  CHECK(has_line(report, "FAIL transport.adapter status=3"));
  CHECK(has_line(report, "ok suite.close_adapter"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(guest_suite, an_adapter_of_another_wddm_version_or_that_will_not_say_fails_that_step)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, 3100u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
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
      kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
      kmt.fail_at(call, 1u, 0xC0000001u);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      { GuestSuite suite(kmt, report, 0u);
        CHECK(suite.run() != 0u);
        CHECK(has_line(report, "code=0xC0000001")); }
      CHECK(report.invariants_hold());
  } }

TEST(guest_suite, each_scenario_stops_at_its_first_failure_and_still_tears_down)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
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

TEST(vsync_scenario, counts_the_blanks_of_a_window_and_judges_the_rate)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VsyncScenario vsync(kmt, report, search);
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK(!vsync.paced());
  vsync.run();
  CHECK(has_line(report, "ok vsync.device"));
  CHECK(has_line(report, "ok vsync.blanks"));
  CHECK(has_line(report, "ok vsync.rate"));
  CHECK(has_line(report, "ok vsync.teardown"));
  CHECK_EQ(vsync.rate_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK_EQ(vsync.caught_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK(vsync.paced());
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank), VsyncScenario::counted_blanks + 1u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(vsync_scenario, an_interval_outside_one_frame_to_two_fails_the_step)
{ const std::uint64_t rates[] = {25u, 120u, 50u, 63u};
  for (const std::uint64_t hertz : rates) {
      MockKmt kmt;
      kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
      kmt.set_blank_period(MockKmt::ticks_per_second / hertz);
      AdapterSearch search(kmt);
      CHECK_STATUS(search.find_avgpu(0u), Status::ok);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      VsyncScenario vsync(kmt, report, search);
      vsync.run();
      const bool within = hertz == 50u || hertz == 63u;
      CHECK_EQ(has_line(report, "ok vsync.rate"), within);
      CHECK_EQ(vsync.paced(), within);
      CHECK_STATUS(search.close(), Status::ok);
  } }

TEST(vsync_scenario, a_refused_or_timed_out_wait_stops_the_count)
{ const std::uint32_t codes[] = {0xC0000001u, 0xC00000B5u};
  for (const std::uint32_t code : codes) {
      MockKmt kmt;
      kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
      kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 5u, code);
      AdapterSearch search(kmt);
      CHECK_STATUS(search.find_avgpu(0u), Status::ok);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
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
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK(has_line(report, "FAIL vsync.device"));
  CHECK(!has_line(report, "vsync.blanks"));
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank), 0u);
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK_STATUS(search.close(), Status::ok); }

TEST(vsync_scenario, without_the_adapter_it_calls_nothing)
{ MockKmt kmt;
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  AdapterSearch search(kmt);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK(has_line(report, "FAIL vsync.adapter"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }

TEST(vsync_scenario, a_clock_that_stood_still_is_no_rate)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.set_blank_period(0u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK(has_line(report, "FAIL vsync.rate"));
  CHECK_STATUS(search.close(), Status::ok); }

TEST(vsync_scenario, judges_the_shortest_interval_not_the_average)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.slip_every(4u, MockKmt::ticks_per_second / VsyncScenario::expected_hz);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK_EQ(vsync.rate_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK(vsync.paced());
  CHECK(vsync.caught_centihertz() < VsyncScenario::expected_hz * 100u);
  CHECK(has_line(report, "ok vsync.rate"));
  CHECK_STATUS(search.close(), Status::ok); }

TEST(vsync_scenario, a_refusal_at_the_first_blank_counts_none)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 1u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK(has_line(report, "FAIL vsync.blanks"));
  CHECK_EQ(vsync.rate_centihertz(), 0u);
  CHECK_EQ(vsync.caught_centihertz(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank), 1u);
  CHECK_STATUS(search.close(), Status::ok); }

TEST(vsync_scenario, a_later_interval_shorter_than_the_first_is_the_one_judged)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.slip_every(2u, MockKmt::ticks_per_second / VsyncScenario::expected_hz);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  VsyncScenario vsync(kmt, report, search);
  vsync.run();
  CHECK_EQ(vsync.rate_centihertz(), VsyncScenario::expected_hz * 100u);
  CHECK(vsync.paced());
  CHECK_STATUS(search.close(), Status::ok); }

TEST(fence_stress_scenario, runs_until_its_seconds_are_spent_and_reports_what_it_saw)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.spend_on_wait(MockKmt::ticks_per_second / 100u);
  kmt.set_blank_period(0u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "ok stress.context"));
  CHECK(has_line(report, "ok stress.fences 200\n"));
  CHECK(has_line(report, "ok stress.worst_wait_us 10000\n"));
  CHECK(has_line(report, "ok stress.teardown"));
  CHECK_EQ(stress.completed(), 200u);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_vertical_blank),
           200u / av::FenceStressScenario::fences_per_blank);
  CHECK_EQ(stress.worst_wait_microseconds(), 10000u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(fence_stress_scenario, no_seconds_is_no_run)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 0u);
  stress.run();
  CHECK_EQ(report.length(), 0u);
  CHECK_EQ(stress.completed(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::submit), 0u);
  CHECK_STATUS(search.close(), Status::ok); }

TEST(fence_stress_scenario, without_an_adapter_it_asks_the_driver_for_nothing)
{ MockKmt kmt;
  AdapterSearch search(kmt);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "FAIL stress.adapter"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(stress.completed(), 0u); }

TEST(fence_stress_scenario, a_device_that_will_not_open_stops_the_run_at_once)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000017u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "FAIL stress.device"));
  CHECK(!has_line(report, "stress.allocation"));
  CHECK(!has_line(report, "stress.fences"));
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(fence_stress_scenario, a_setup_that_fails_never_reaches_the_fences)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::create_context, 1u, 0xC0000017u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "FAIL stress.context"));
  CHECK(!has_line(report, "stress.fences"));
  CHECK_EQ(stress.completed(), 0u);
  CHECK_EQ(stress.worst_wait_microseconds(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::submit), 0u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(fence_stress_scenario, a_submission_that_fails_stops_the_run)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.spend_on_wait(MockKmt::ticks_per_second / 100u);
  kmt.set_blank_period(0u);
  kmt.fail_at(MockKmt::Call::submit, 3u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "FAIL stress.fences"));
  CHECK_EQ(stress.completed(), 2u);
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_context), 2u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(fence_stress_scenario, a_blank_that_never_comes_ends_the_run)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.spend_on_wait(MockKmt::ticks_per_second / 100u);
  kmt.set_blank_period(0u);
  kmt.fail_at(MockKmt::Call::wait_for_vertical_blank, 1u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "FAIL stress.fences"));
  CHECK_EQ(stress.completed(), av::FenceStressScenario::fences_per_blank);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(fence_stress_scenario, a_wait_that_fails_stops_the_run_and_is_reported)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.spend_on_wait(MockKmt::ticks_per_second / 100u);
  kmt.fail_at(MockKmt::Call::wait_for_context, 4u, 0xC0000001u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  FenceStressScenario stress(kmt, report, search, 2u);
  stress.run();
  CHECK(has_line(report, "FAIL stress.fences"));
  CHECK_EQ(stress.completed(), 3u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(timeout_scenario, the_held_context_is_lost_and_the_other_completes)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::wait_for_context, 1u, device_removed);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TimeoutScenario timeout(kmt, report, search);
  timeout.run();
  CHECK(has_line(report, "ok timeout.held_lost"));
  CHECK(has_line(report, "ok timeout.other_completed"));
  CHECK(has_line(report, "ok timeout.held_set_up"));
  CHECK(has_line(report, "ok timeout.other_set_up"));
  CHECK_EQ(kmt.calls(MockKmt::Call::submit), 2u);
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(timeout_scenario, a_held_context_that_completes_fails_the_run)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TimeoutScenario timeout(kmt, report, search);
  timeout.run();
  CHECK(has_line(report, "FAIL timeout.held_lost"));
  CHECK(has_line(report, "ok timeout.other_completed"));
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(timeout_scenario, the_other_context_being_lost_fails_the_run)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::wait_for_context, 2u, device_removed);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TimeoutScenario timeout(kmt, report, search);
  timeout.run();
  CHECK(has_line(report, "FAIL timeout.other_completed"));
  CHECK_STATUS(search.close(), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(guest_suite, the_timeout_run_is_the_timeout_scenario_and_the_adapter)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::wait_for_context, 1u, device_removed);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  CHECK_EQ(suite.run_timeout(), 0u);
  CHECK(has_line(report, "ok adapter.find"));
  CHECK(has_line(report, "ok timeout.held_lost"));
  CHECK(std::strstr(report.text(), "allocation.") == nullptr);
  CHECK(std::strstr(report.text(), "submission.") == nullptr);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(timeout_scenario, without_the_adapter_it_reports_and_stops)
{ MockKmt kmt;
  AdapterSearch search(kmt);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TimeoutScenario timeout(kmt, report, search);
  timeout.run();
  CHECK(has_line(report, "FAIL timeout.adapter"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }

TEST(timeout_scenario, a_setup_that_fails_stops_the_run_where_it_failed)
{ const struct { MockKmt::Call call; std::uint32_t nth; const char *line; } cases[] = {
      {MockKmt::Call::create_device, 1u, "FAIL timeout.held_set_up"},
      {MockKmt::Call::create_device, 2u, "FAIL timeout.other_set_up"},
      {MockKmt::Call::submit, 1u, "FAIL timeout.held_submit"},
      {MockKmt::Call::submit, 2u, "FAIL timeout.other_submit"},
  };
  for (const auto &one : cases) {
      MockKmt kmt;
      kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
      kmt.fail_at(one.call, one.nth, 0xC0000001u);
      AdapterSearch search(kmt);
      CHECK_STATUS(search.find_avgpu(0u), Status::ok);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      TimeoutScenario timeout(kmt, report, search);
      timeout.run();
      CHECK(has_line(report, one.line));
      CHECK(!has_line(report, "timeout.held_lost"));
      CHECK_STATUS(search.close(), Status::ok);
      CHECK_EQ(kmt.open_objects(), 0u);
  } }

TEST(guest_suite_run_av, runs_only_the_av_scenarios_in_order_and_closes_the_adapter)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.allow_hardware_scheduling();
  kmt.open_host_window(2u * HostVisibleImportScenario::chunk_bytes);
  av::ScriptedHost host(kmt);
  host.transport_run();
  host.submit_run();
  host.vulkan_run();
  host.opencl_run();
  host.host_visible_import_run();
  host.transport_run();
  host.healthy_context();
  const std::uint32_t before_import = av::ScriptedHost::transport_waits + av::ScriptedHost::submit_waits +
                                     av::ScriptedHost::vulkan_waits + av::ScriptedHost::opencl_waits;
  kmt.play_at_wait(before_import + av::ScriptedHost::host_visible_import_copy_wait, [](void *played) {
      MockKmt &mock = *static_cast<MockKmt *>(played);
      std::memcpy(mock.window_chunk_memory(MockKmt::first_window_resource + 1u),
                  mock.window_chunk_memory(MockKmt::first_window_resource), HostVisibleImportScenario::copy_bytes);
  }, &kmt);
  const std::uint32_t before_fault = before_import + av::ScriptedHost::host_visible_import_waits +
                                     av::ScriptedHost::transport_waits + av::ScriptedHost::isolation_waits_before_fault;
  kmt.withhold_answer_at(before_fault + 1u);
  host.healthy_context();
  host.nop();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  const std::uint32_t failed = suite.run_av();
  CHECK_EQ(failed, 0u);
  const char *const order[] = {"ok adapter.find\n", "ok transport.nop 0\n", "ok submit.kernel_ns 0\n", "ok vulkan.bind\n",
                               "ok opencl.sum 64\n", "ok import.copied 1048576\n", "ok import.release\n",
                               "ok opengl.nop 0\n", "ok isolation.relink\n",
                               "ok suite.close_adapter\n", "avtest: 85 passed, 0 failed\n"};
  const char *at = report.text();
  for (const char *line : order) {
      const char *const found = std::strstr(at, line);
      CHECK(found != nullptr);
      if (found != nullptr) at = found;
  }
  CHECK(std::strstr(report.text(), "allocation.") == nullptr);
  CHECK(std::strstr(report.text(), "vsync.") == nullptr);
  CHECK_EQ(kmt.queued_answers(), 0u);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(guest_suite_run_av, without_the_adapter_every_av_scenario_fails_at_its_first_step)
{ MockKmt kmt;
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  CHECK_EQ(suite.run_av(), 8u);
  const char *const lines[] = {"FAIL adapter.find", "FAIL transport.adapter status=3",
                               "FAIL vulkan.adapter status=3", "FAIL opencl.adapter status=3",
                               "FAIL import.adapter status=3", "FAIL opengl.adapter status=3",
                               "FAIL isolation.adapter status=3",
                               "FAIL submit.adapter status=3"};
  for (const char *line : lines) CHECK(std::strstr(report.text(), line) != nullptr);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }

TEST(guest_suite_run_round_trip, measures_answered_no_ops_on_the_adapter_it_names)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.set_adapter_luid(0u, 0x1234u);
  kmt.answer_waits(nullptr, 0u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  CHECK_EQ(suite.run_round_trip(), 0u);
  CHECK(has_line(report, "ok adapter.find\n"));
  CHECK(has_line(report, "ok trip.adapter_luid 4660\n"));
  CHECK(has_line(report, "ok submit.trip_median_ns"));
  CHECK(has_line(report, "ok suite.close_adapter\n"));
  CHECK(!has_line(report, "allocation."));
  CHECK(!has_line(report, "vsync."));
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(guest_suite_run_round_trip, a_failed_no_op_is_counted_among_the_failures)
{ MockKmt kmt;
  kmt.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
  kmt.answer_waits(nullptr, 0u);
  kmt.fail_at(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  GuestSuite suite(kmt, report, 0u);
  CHECK_EQ(suite.run_round_trip(), 1u);
  CHECK(has_line(report, "FAIL submit.warm"));
  CHECK_EQ(kmt.open_objects(), 0u); }
