/* The transport round trip over MockKmt, the test playing the echo executor by
 * scripting the answer the device writes to the reply slot: an echo that comes
 * back passes, an answer that is not the payload fails the reply, a failure at
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

/* The payload as the wire carries it, least significant byte first. */
void echo_of_payload(std::uint8_t *bytes)
{
    for (std::uint32_t i = 0u; i < TransportScenario::payload_bytes; i++)
        bytes[i] = static_cast<std::uint8_t>(TransportScenario::echo_payload >> (8u * i));
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

TEST(transport_scenario, an_echo_that_comes_back_passes_every_step)
{ Rig rig;
  std::uint8_t echo[TransportScenario::payload_bytes] = {};
  echo_of_payload(echo);
  rig.kmt().answer_waits(echo, sizeof echo);
  GuestReport report;
  TransportScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(std::strcmp(report.text(),
                    "ok transport.adapter\n"
                    "ok transport.connection\n"
                    "ok transport.initialize\n"
                    "ok transport.send_sync\n"
                    "ok transport.reply 8\n") == 0);
  CHECK_EQ(report.failed(), 0u);
  /* The connection used the adapter the search kept, never enumerating again. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 1u);
  CHECK(report.invariants_hold());
  rig.check_nothing_left_but_the_adapter(); }

/* An answer that is not exactly the payload fails the reply: a byte flipped, a
 * short answer, and the payload with more after it. The number reported is
 * the bytes of the payload the answer reproduced. */
TEST(transport_scenario, an_answer_that_is_not_the_payload_fails_the_reply)
{ struct Case { std::uint32_t size; std::uint32_t flip; const char *line; };
  const Case cases[] = {
      {8u, 3u, "FAIL transport.reply status=9 7\n"},
      {4u, 8u, "FAIL transport.reply status=9 0\n"},
      {16u, 8u, "FAIL transport.reply status=9 8\n"},
      {0u, 8u, "FAIL transport.reply status=9 0\n"}};
  for (const Case &wrong : cases) {
      Rig rig;
      std::uint8_t answer[16] = {};
      echo_of_payload(answer);
      echo_of_payload(answer + 8u);
      if (wrong.flip < 8u) answer[wrong.flip] ^= 0x01u;
      rig.kmt().answer_waits(answer, wrong.size);
      GuestReport report;
      TransportScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      CHECK(has_line(report, "ok transport.send_sync\n"));
      CHECK(has_line(report, wrong.line));
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
      {MockKmt::Call::create_allocation, "FAIL transport.initialize status=7", "transport.send_sync"},
      {MockKmt::Call::lock, "FAIL transport.initialize status=7", "transport.send_sync"},
      {MockKmt::Call::create_context_virtual, "FAIL transport.initialize status=7", "transport.send_sync"},
      {MockKmt::Call::submit, "FAIL transport.send_sync status=7 code=0xC0000001\n", "transport.reply"},
      {MockKmt::Call::wait_for_context, "FAIL transport.send_sync status=7 code=0xC0000001\n", "transport.reply"}};
  for (const Case &refusal : cases) {
      Rig rig;
      std::uint8_t echo[TransportScenario::payload_bytes] = {};
      echo_of_payload(echo);
      rig.kmt().answer_waits(echo, sizeof echo);
      rig.kmt().fail_at(refusal.call, 1u, unsuccessful);
      GuestReport report;
      TransportScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      CHECK(has_line(report, refusal.line));
      CHECK(!has_line(report, refusal.skipped));
      CHECK(!has_line(report, "transport.reply"));
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
  CHECK(has_line(report, "FAIL transport.send_sync status=7"));
  CHECK(!has_line(report, "transport.reply"));
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
