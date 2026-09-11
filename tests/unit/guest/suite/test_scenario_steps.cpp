#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/guest_session.hpp"
#include "guest/suite/scenario_steps.hpp"
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
  GuestReport report;
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
  GuestReport report;
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
