/* Fault isolation over MockKmt, the test playing the host through ScriptedHost
 * and faulting the second context as the driver leaves it: its wait completes
 * with no reply written, since the driver completes a faulted context's work
 * and never reports DMA_FAULTED (dxgmms2.sys bugchecks 0x119/0xD on it):
 * the healthy context works before and after, the faulted one's next send
 * fails, a context made afterwards works, and every step's failure stops the
 * run with every context torn down. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/isolation_scenario.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::CommandHeader;
using av::GuestReport;
using av::IsolationScenario;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using namespace av::vkproto;

namespace {

const PciIdentity av(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
/* What D3DKMT answers a wait on a device that was removed. */
constexpr std::uint32_t device_removed = 0xC00E0102u;
/* The healthy context's three waits, then the faulted command's. */
constexpr std::uint32_t fault_wait = ScriptedHost::isolation_waits_before_fault + 1u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

class Rig {
public:
    Rig() : search_(kmt_), host_(kmt_)
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }
    ScriptedHost  &host() { return host_; }

    void check_nothing_left_but_the_adapter()
    {
        CHECK_EQ(kmt_.queued_answers(), 0u);
        CHECK_EQ(kmt_.open_objects(), 1u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
    }

private:
    MockKmt       kmt_;
    AdapterSearch search_;
    ScriptedHost  host_;
};

}  // namespace

TEST(isolation_scenario, a_faulted_context_changes_no_other)
{ Rig rig;
  rig.host().healthy_context();
  rig.kmt().withhold_answer_at(fault_wait);
  rig.host().healthy_context();
  rig.host().nop();
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  IsolationScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(std::strcmp(report.text(),
                    "ok isolation.adapter\n"
                    "ok isolation.connection\n"
                    "ok isolation.initialize\n"
                    "ok isolation.healthy_before\n"
                    "ok isolation.fault\n"
                    "ok isolation.faulted_send\n"
                    "ok isolation.healthy_after\n"
                    "ok isolation.relink\n") == 0);
  const std::uint32_t expected[] = {CommandHeader::nop_id, kCmd_vkCreateInstance, kCmd_vkDestroyInstance,
                                    CommandHeader::nop_id, kCmd_vkCreateInstance, CommandHeader::nop_id,
                                    kCmd_vkCreateInstance, kCmd_vkDestroyInstance, CommandHeader::nop_id,
                                    CommandHeader::nop_id};
  CHECK_EQ(rig.kmt().commands(), sizeof expected / sizeof expected[0]);
  for (std::uint32_t index = 0u; index < sizeof expected / sizeof expected[0]; index++)
      CHECK_EQ(rig.kmt().command_id(index), expected[index]);
  /* The malformed command is its structure type alone, and the faulted
   * context sent nothing after it. */
  CHECK_EQ(rig.kmt().command_size(4u), CommandHeader::encoded_size + sizeof(std::uint32_t));
  /* Three contexts, on one device. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 3u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 1u);
  CHECK_EQ(report.failed(), 0u);
  rig.check_nothing_left_but_the_adapter(); }

/* A healthy context that is not healthy stops the run before any fault: its
 * no-op lost, its instance refused, or the no-op after the destroy lost. */
TEST(isolation_scenario, a_healthy_context_that_fails_first_stops_the_run)
{ struct Case { std::uint32_t answers; bool refuse_instance; };
  const Case cases[] = {{0u, false}, {1u, true}, {2u, false}};
  for (const Case &failure : cases) {
      Rig rig;
      if (failure.answers >= 1u) rig.host().nop();
      if (failure.refuse_instance) rig.host().vk_handle(-9, 0u);
      if (failure.answers == 2u) rig.host().vk_handle(0, ScriptedHost::vk_instance);
      rig.kmt().fail_at(MockKmt::Call::wait_for_context, failure.refuse_instance ? 99u : failure.answers + 1u,
                        device_removed);
      av::MockPlatform report_memory;
      GuestReport report(report_memory);
      IsolationScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      CHECK(has_line(report, "FAIL isolation.healthy_before"));
      CHECK(!has_line(report, "isolation.fault"));
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 1u);
      CHECK_EQ(report.failed(), 1u);
      rig.check_nothing_left_but_the_adapter();
  } }

/* A host that runs the malformed command fails the step: isolation was never
 * shown. So does a second context that cannot be set up. */
TEST(isolation_scenario, a_fault_that_never_happens_fails_fault)
{ { Rig rig;
    rig.host().healthy_context();
    rig.host().vk_handle(0, ScriptedHost::vk_instance);
    av::MockPlatform report_memory;
    GuestReport report(report_memory);
    IsolationScenario scenario(rig.kmt(), report, rig.search());
    scenario.run();
    CHECK(has_line(report, "FAIL isolation.fault status=4"));
    CHECK(!has_line(report, "isolation.faulted_send"));
    rig.check_nothing_left_but_the_adapter(); }
  { Rig rig;
    rig.host().healthy_context();
    rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 2u, 0xC0000001u);
    av::MockPlatform report_memory;
    GuestReport report(report_memory);
    IsolationScenario scenario(rig.kmt(), report, rig.search());
    scenario.run();
    CHECK(has_line(report, "FAIL isolation.fault status=7"));
    CHECK_EQ(rig.kmt().commands(), 4u);   /* the healthy context's, and none from the second */
    rig.check_nothing_left_but_the_adapter(); } }

/* After the fault: the healthy context lost, a relink that cannot be set up,
 * and one whose no-op is lost each fail their own step. */
TEST(isolation_scenario, a_failure_after_the_fault_is_reported_where_it_happened)
{ { Rig rig;
    rig.host().healthy_context();
    rig.kmt().withhold_answer_at(fault_wait);
    rig.host().nop();
    rig.host().vk_handle(-3, 0u);
    av::MockPlatform report_memory;
    GuestReport report(report_memory);
    IsolationScenario scenario(rig.kmt(), report, rig.search());
    scenario.run();
    CHECK(has_line(report, "ok isolation.faulted_send\n"));
    CHECK(has_line(report, "FAIL isolation.healthy_after status=7"));
    CHECK(!has_line(report, "isolation.relink"));
    rig.check_nothing_left_but_the_adapter(); }
  { Rig rig;
    rig.host().healthy_context();
    rig.kmt().withhold_answer_at(fault_wait);
    rig.host().healthy_context();
    rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 3u, 0xC0000001u);
    av::MockPlatform report_memory;
    GuestReport report(report_memory);
    IsolationScenario scenario(rig.kmt(), report, rig.search());
    scenario.run();
    CHECK(has_line(report, "ok isolation.healthy_after\n"));
    CHECK(has_line(report, "FAIL isolation.relink status=7"));
    rig.check_nothing_left_but_the_adapter(); }
  { Rig rig;
    rig.host().healthy_context();
    rig.kmt().withhold_answer_at(fault_wait);
    rig.host().healthy_context();
    rig.host().vk_result(0);   /* bytes where the relinked context's no-op answers none */
    av::MockPlatform report_memory;
    GuestReport report(report_memory);
    IsolationScenario scenario(rig.kmt(), report, rig.search());
    scenario.run();
    CHECK(has_line(report, "FAIL isolation.relink status=9"));
    CHECK_EQ(report.failed(), 1u);
    rig.check_nothing_left_but_the_adapter(); } }

TEST(isolation_scenario, without_the_adapter_it_asks_the_driver_for_nothing)
{ MockKmt kmt;
  AdapterSearch search(kmt);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  IsolationScenario scenario(kmt, report, search);
  scenario.run();
  CHECK(has_line(report, "FAIL isolation.adapter status=3"));
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u); }
