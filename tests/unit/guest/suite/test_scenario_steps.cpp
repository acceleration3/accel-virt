#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_session.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstring>

using av::GuestReport;
using av::GuestSession;
using av::PciIdentity;
using av::MockKmt;
using av::ScenarioSteps;
using av::Status;

TEST(scenario_steps, report_each_step_with_the_last_call_s_ntstatus)
{ MockKmt kmt;
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  ScenarioSteps steps(kmt, report, "probe");
  std::uint32_t device = 0u;
  kmt.fail_at(MockKmt::Call::create_device, 1u, 0xC0000017u);
  CHECK_STATUS(steps.check("device", kmt.create_device(1u, device)), Status::no_memory);
  steps.check_refused("hostile", kmt.create_device(1u, device));
  CHECK_STATUS(steps.check("fine", Status::ok), Status::ok);
  CHECK(std::strcmp(report.text(),
                    "FAIL probe.device status=2 code=0xC0000017\n"
                    "ok probe.hostile\n"
                    "ok probe.fine\n") == 0); }

TEST(scenario_steps, a_teardown_is_recorded_with_its_first_failure_s_ntstatus)
{ MockKmt kmt;
  kmt.add_adapter(PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  ScenarioSteps steps(kmt, report, "probe");
  GuestSession session(kmt, adapter);
  CHECK_STATUS(session.open_device(), Status::ok);
  kmt.fail_at(MockKmt::Call::destroy_device, 1u, 0xC0000001u);
  steps.check_teardown(session);
  steps.check_teardown(session);   /* the refused device is not kept: nothing left to destroy */
  CHECK(std::strcmp(report.text(),
                    "FAIL probe.teardown status=7 code=0xC0000001\n"
                    "ok probe.teardown\n") == 0);
  CHECK_STATUS(kmt.close_adapter(adapter), Status::ok); }

/* A rate is not an NTSTATUS: the measurement is on the line either way, because
 * the number is what the step was for and the runner reads it back. */
TEST(scenario_steps_check_measured, puts_the_measurement_on_the_line_passed_or_not)
{ MockKmt kmt;
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  ScenarioSteps steps(kmt, report, "vsync");
  CHECK_STATUS(steps.check_measured("rate", Status::conflict, 3098u), Status::conflict);
  CHECK(std::strstr(report.text(), "FAIL vsync.rate status=4 3098") != nullptr);
  CHECK_STATUS(steps.check_measured("rate", Status::ok, 6000u), Status::ok);
  CHECK(std::strstr(report.text(), "ok vsync.rate 6000") != nullptr); }

/* A graphics API's result is not an NTSTATUS either: success is a bare line, and
 * a refusal carries the API's own code - VK_ERROR_OUT_OF_DEVICE_MEMORY (-2) as
 * its bits, VK_INCOMPLETE (5) as itself - with the status a failed step has. */
TEST(scenario_steps_check_result, zero_passes_and_anything_else_fails_with_the_code_on_the_line)
{ MockKmt kmt;
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  ScenarioSteps steps(kmt, report, "vulkan");
  CHECK_STATUS(steps.check_result("instance", 0), Status::ok);
  CHECK_STATUS(steps.check_result("memory", -2), Status::invalid_state);
  CHECK_STATUS(steps.check_result("incomplete", 5), Status::invalid_state);
  CHECK(std::strcmp(report.text(),
                    "ok vulkan.instance\n"
                    "FAIL vulkan.memory status=7 4294967294\n"
                    "FAIL vulkan.incomplete status=7 5\n") == 0);
  CHECK_EQ(report.passed(), 1u);
  CHECK_EQ(report.failed(), 2u); }

/* Feature 053: a step whose kernel call was made by somebody else's Kmt. A
 * probe calling a driver through an API of its own never touches the kernel
 * itself, so its own last_code() is 0 and reporting that would say no kernel
 * call was involved - the opposite of the truth. */
TEST(scenario_steps, check_kernel_reports_a_status_this_kmt_never_answered)
{ MockKmt       kmt;
  av::MockPlatform report_memory;
  GuestReport   report(report_memory, "probe");
  ScenarioSteps steps(kmt, report, "shared");
  CHECK_EQ(kmt.last_code(), 0u);
  CHECK_STATUS(steps.check_kernel("refused", av::Status::invalid_state, 0xC000000Du),
               av::Status::invalid_state);
  CHECK_STATUS(steps.check_kernel("taken", av::Status::ok, 0u), av::Status::ok);
  report.finish();
  CHECK(std::strstr(report.text(), "FAIL shared.refused") != nullptr);
  CHECK(std::strstr(report.text(), "code=0xC000000D") != nullptr);
  CHECK(std::strstr(report.text(), "ok shared.taken") != nullptr); }
