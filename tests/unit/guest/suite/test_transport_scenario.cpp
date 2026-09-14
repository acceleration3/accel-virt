/* The transport round trip over MockKmt, the test playing the device by
 * scripting the answer it writes to the reply slot: a no-op answered empty
 * passes with its latency, an answer holding bytes fails the no-op, a failure at
 * any earlier step is reported with every later step skipped, and nothing is
 * left open but the adapter the search owns. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/transport_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::TransportScenario;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
/* Microsoft Basic Render, enumerated before rvk as it is in a real guest. */
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
constexpr std::uint32_t unsuccessful = 0xC0000001u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

/* A guest with both adapters, the search run, and an answer scripted. */
class Rig {
public:
    Rig() : search_(kmt_)
    {
        kmt_.add_adapter(basic_render, 3100u);
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find(rvk), Status::ok);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }

    /* After a run only the search's adapter may be open, and no call may have
     * named a handle the driver never gave. */
    void check_nothing_left_but_the_adapter()
    {
        CHECK_EQ(kmt_.open_objects(), 1u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
    }

private:
    MockKmt       kmt_;
    AdapterSearch search_;
};

}  // namespace

TEST(transport_scenario, a_no_op_the_device_answers_empty_passes_and_reports_its_latency)
{ Rig rig;
  rig.kmt().answer_waits(nullptr, 0u);
  rig.kmt().spend_on_wait(2500u);   /* a quarter of a millisecond at 10 000 000 ticks a second */
  GuestReport report;
  TransportScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(std::strcmp(report.text(),
                    "ok transport.adapter\n"
                    "ok transport.connection\n"
                    "ok transport.initialize\n"
                    "ok transport.nop 0\n"
                    "ok transport.latency 250\n") == 0);
  CHECK_EQ(report.failed(), 0u);
  /* The connection used the adapter the search kept, never enumerating again. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 1u);
  CHECK(report.invariants_hold());
  rig.check_nothing_left_but_the_adapter(); }

/* A no-op is answered with nothing: an answer holding bytes is some other
 * command's, and the number reported is how many there were. */
TEST(transport_scenario, an_answer_with_bytes_in_it_is_not_a_no_op)
{ const std::uint32_t sizes[] = {1u, 8u, 64u};
  for (const std::uint32_t size : sizes) {
      Rig rig;
      std::uint8_t answer[64] = {};
      rig.kmt().answer_waits(answer, size);
      GuestReport report;
      TransportScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      char line[64];
      static_cast<void>(std::snprintf(line, sizeof line, "FAIL transport.nop status=9 %u\n", size));
      CHECK(has_line(report, line));
      CHECK(!has_line(report, "transport.latency"));
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

/* Each step's failure is the last line: nothing after it is attempted, and
 * everything before it is torn down. */
TEST(transport_scenario, a_failure_at_each_step_is_reported_and_later_steps_skipped)
{ struct Case { MockKmt::Call call; const char *line; const char *skipped; };
  const Case cases[] = {
      {MockKmt::Call::create_device, "FAIL transport.connection status=7 code=0xC0000001\n", "transport.initialize"},
      {MockKmt::Call::create_paging_queue, "FAIL transport.connection status=7", "transport.initialize"},
      {MockKmt::Call::create_allocation, "FAIL transport.initialize status=7", "transport.nop"},
      {MockKmt::Call::lock, "FAIL transport.initialize status=7", "transport.nop"},
      {MockKmt::Call::create_context_virtual, "FAIL transport.initialize status=7", "transport.nop"},
      {MockKmt::Call::submit, "FAIL transport.nop status=7", "transport.latency"},
      {MockKmt::Call::wait_for_context, "FAIL transport.nop status=7", "transport.latency"}};
  for (const Case &refusal : cases) {
      Rig rig;
      rig.kmt().answer_waits(nullptr, 0u);
      rig.kmt().fail_at(refusal.call, 1u, unsuccessful);
      GuestReport report;
      TransportScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      CHECK(has_line(report, refusal.line));
      CHECK(!has_line(report, refusal.skipped));
      CHECK(!has_line(report, "transport.latency"));
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

/* A device that never writes the answer loses the call, not the guest. */
TEST(transport_scenario, a_device_that_never_answers_fails_the_send)
{ Rig rig;
  GuestReport report;
  TransportScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok transport.initialize\n"));
  CHECK(has_line(report, "FAIL transport.nop status=7"));
  CHECK(!has_line(report, "transport.latency"));
  rig.check_nothing_left_but_the_adapter(); }

TEST(transport_scenario, without_the_adapter_it_asks_the_driver_for_nothing)
{ MockKmt kmt;
  kmt.add_adapter(basic_render, 3100u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find(rvk), Status::not_found);
  GuestReport report;
  TransportScenario scenario(kmt, report, search);
  scenario.run();
  CHECK(has_line(report, "FAIL transport.adapter status=3"));
  CHECK_EQ(report.passed(), 0u);
  CHECK_EQ(report.failed(), 1u);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }
