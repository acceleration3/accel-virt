#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/transport_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/escape/av_context_record.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::TransportScenario;

namespace {

const PciIdentity av_gpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
const PciIdentity basic_render(0x1414u, 0x008Cu, 0u, 0u);
constexpr std::uint32_t unsuccessful = 0xC0000001u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

class Rig {
public:
    Rig() : search_(kmt_)
    {
        kmt_.add_adapter(basic_render, 3100u);
        kmt_.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }

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

}

TEST(transport_scenario, a_no_op_the_device_answers_empty_passes_and_reports_its_latency)
{ Rig rig;
  rig.kmt().answer_waits(nullptr, 0u);
  rig.kmt().spend_on_wait(2500u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
  scenario.run();
  CHECK(std::strcmp(report.text(),
                    "ok transport.adapter\n"
                    "ok transport.connection\n"
                    "ok transport.initialize\n"
                    "ok transport.nop 0\n"
                    "ok transport.latency 1250\n"
                    "ok transport.warm_nop 31\n"
                    "ok transport.warm_median 1250\n"
                    "ok transport.warm_fastest 1250\n") == 0);
  CHECK_EQ(report.failed(), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 32u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 32u);
  CHECK(report.invariants_hold());
  rig.check_nothing_left_but_the_adapter(); }

TEST(transport_scenario, an_answer_with_bytes_in_it_is_not_a_no_op)
{ const std::uint32_t sizes[] = {1u, 8u, 64u};
  for (const std::uint32_t size : sizes) {
      Rig rig;
      std::uint8_t answer[64] = {};
      rig.kmt().answer_waits(answer, size);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
      scenario.run();
      char line[64];
      static_cast<void>(std::snprintf(line, sizeof line, "FAIL transport.nop status=9 %u\n", size));
      CHECK(has_line(report, line));
      CHECK(!has_line(report, "transport.latency"));
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(transport_scenario, a_failure_at_each_step_is_reported_and_later_steps_skipped)
{ struct Case { MockKmt::Call call; const char *line; const char *skipped; };
  const Case cases[] = {
      {MockKmt::Call::create_device, "FAIL transport.connection status=7 code=0xC0000001\n", "transport.initialize"},
      {MockKmt::Call::create_paging_queue, "FAIL transport.connection status=7", "transport.initialize"},
      {MockKmt::Call::create_allocation, "FAIL transport.initialize status=7", "transport.nop"},
      {MockKmt::Call::escape, "FAIL transport.initialize status=7", "transport.nop"},
      {MockKmt::Call::create_context_virtual, "FAIL transport.initialize status=7", "transport.nop"},
      {MockKmt::Call::submit, "FAIL transport.nop status=7", "transport.latency"},
      {MockKmt::Call::wait_for_context, "FAIL transport.nop status=7", "transport.latency"}};
  for (const Case &refusal : cases) {
      Rig rig;
      rig.kmt().answer_waits(nullptr, 0u);
      rig.kmt().fail_at(refusal.call, 1u, unsuccessful);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
      scenario.run();
      CHECK(has_line(report, refusal.line));
      CHECK(!has_line(report, refusal.skipped));
      CHECK(!has_line(report, "transport.latency"));
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(transport_scenario, the_warm_no_ops_report_their_median_and_fastest)
{ struct Case { std::uint32_t slow_nth; const char *median; const char *fastest; const char *first; };
  const Case cases[] = {{3u, "ok transport.warm_median 2000\n", "ok transport.warm_fastest 2000\n",
                         "ok transport.latency 2000\n"},
                        {2u, "ok transport.warm_median 7000\n", "ok transport.warm_fastest 2000\n",
                         "ok transport.latency 2000\n"}};
  for (const Case &one : cases) {
      Rig rig;
      rig.kmt().answer_waits(nullptr, 0u);
      rig.kmt().spend_on_wait(10000u);
      rig.kmt().slow_every(one.slow_nth, 50000u);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
      scenario.run();
      CHECK(has_line(report, one.first));
      CHECK(has_line(report, "ok transport.warm_nop 31\n"));
      CHECK(has_line(report, one.median));
      CHECK(has_line(report, one.fastest));
      CHECK_EQ(report.failed(), 0u);
      rig.check_nothing_left_but_the_adapter();
  } }

TEST(transport_scenario, a_warm_no_op_that_fails_stops_the_warm_rounds)
{ Rig rig;
  rig.kmt().queue_answer(nullptr, 0u);
  const std::uint8_t warm_answer[4] = {};
  rig.kmt().queue_answer(warm_answer, sizeof warm_answer);
  rig.kmt().spend_on_wait(50000u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
  scenario.run();
  CHECK(has_line(report, "ok transport.latency 6000\n"));
  CHECK(has_line(report, "FAIL transport.warm_nop status=9 4\n"));
  CHECK(!has_line(report, "transport.warm_median"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 2u);
  CHECK_EQ(report.failed(), 1u);
  rig.check_nothing_left_but_the_adapter();

  Rig second;
  second.kmt().answer_waits(nullptr, 0u);
  second.kmt().fail_at(MockKmt::Call::wait_for_context, 7u, unsuccessful);
  av::MockPlatform lost_memory;
  GuestReport lost(lost_memory);
  TransportScenario again(second.kmt(), lost, second.search(), av::SubApi::vulkan, "transport");
  again.run();
  CHECK(has_line(lost, "ok transport.latency 1000\n"));
  CHECK(has_line(lost, "FAIL transport.warm_nop status=7"));
  CHECK(!has_line(lost, "transport.warm_fastest"));
  CHECK_EQ(lost.failed(), 1u);
  second.check_nothing_left_but_the_adapter(); }

TEST(transport_scenario, a_clock_without_a_frequency_reports_no_latency)
{ Rig rig;
  rig.kmt().answer_waits(nullptr, 0u);
  rig.kmt().spend_on_wait(2500u);
  rig.kmt().set_frequency(0u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
  scenario.run();
  CHECK(has_line(report, "ok transport.nop 0\n"));
  CHECK(has_line(report, "ok transport.latency 0\n"));
  CHECK(has_line(report, "ok transport.warm_median 0\n"));
  CHECK_EQ(report.failed(), 0u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(transport_scenario, a_device_that_never_answers_fails_the_send)
{ Rig rig;
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::vulkan, "transport");
  scenario.run();
  CHECK(has_line(report, "ok transport.initialize\n"));
  CHECK(has_line(report, "FAIL transport.nop status=7"));
  CHECK(!has_line(report, "transport.latency"));
  rig.check_nothing_left_but_the_adapter(); }

TEST(transport_scenario, without_the_adapter_it_asks_the_driver_for_nothing)
{ MockKmt kmt;
  kmt.add_adapter(basic_render, 3100u);
  AdapterSearch search(kmt);
  CHECK_STATUS(search.find_avgpu(0u), Status::not_found);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TransportScenario scenario(kmt, report, search, av::SubApi::vulkan, "transport");
  scenario.run();
  CHECK(has_line(report, "FAIL transport.adapter status=3"));
  CHECK_EQ(report.passed(), 0u);
  CHECK_EQ(report.failed(), 1u);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(transport_scenario, an_opengl_context_is_asked_for_by_its_record_and_named_in_the_report)
{ Rig rig;
  rig.kmt().answer_waits(nullptr, 0u);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  TransportScenario scenario(rig.kmt(), report, rig.search(), av::SubApi::opengl, "opengl");
  scenario.run();
  CHECK(has_line(report, "ok opengl.initialize\n"));
  CHECK(has_line(report, "ok opengl.nop 0\n"));
  CHECK_EQ(report.failed(), 0u);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::opengl);
  rig.check_nothing_left_but_the_adapter(); }
