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
                    "ok suite.close_adapter\n"
                    "rvktest: 23 passed, 0 failed\n") == 0);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK(report.invariants_hold()); }

TEST(guest_suite, without_the_adapter_every_scenario_fails_without_calling_d3dkmt)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x1100u), 1300u);
  GuestReport report;
  GuestSuite suite(kmt, report);
  CHECK_EQ(suite.run(), 4u);
  CHECK(has_line(report, "FAIL adapter.find status=3"));
  CHECK(has_line(report, "FAIL allocation.adapter status=3"));
  CHECK(has_line(report, "FAIL mapping.adapter status=3"));
  CHECK(has_line(report, "FAIL submission.adapter status=3"));
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
