#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/host_loss_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::CommandHeader;
using av::GuestReport;
using av::HostLossScenario;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;

namespace {

const PciIdentity av_gpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device, PciIdentity::virtio_vendor,
                         AvgpuAdapters::av_subsystem);
constexpr std::uint32_t level = 0xB000u;
constexpr std::uint32_t device_removed = 0x887A0005u;
constexpr std::uint32_t lose_wait = 7u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

void fault_every_context(void *kmt) { static_cast<MockKmt *>(kmt)->fault_every_page(); }

class Rig {
public:
    explicit Rig(bool faults = true) : search_(kmt_), host_(kmt_), report_(report_memory_)
    {
        kmt_.add_adapter(av_gpu, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
        if (faults) kmt_.play_at_wait(lose_wait, &fault_every_context, &kmt_);
    }

    MockKmt      &kmt() { return kmt_; }
    ScriptedHost &host() { return host_; }
    GuestReport  &report() { return report_; }

    void drawing(std::uint32_t device_hresult = 0u, std::uint32_t finish_hresult = 0u, std::uint32_t last = 0u)
    {
        host_.d3d_device(device_hresult, level);
        host_.d3d_finish(finish_hresult);
        host_.d3d_finish(last);
    }
    void idle() { host_.d3d_device(0u, level); host_.d3d_finish(0u); }
    void other_before() { host_.nop(); }
    void lost() { host_.host_loss(CommandHeader::host_device_lost); }
    void after_the_loss(bool drawing_answers = false)
    {
        if (!drawing_answers) host_.cut_next(1u);
        host_.d3d_finish(0u);
        host_.d3d_device(device_removed, level);
        host_.nop();
    }
    void everything() { drawing(); idle(); other_before(); lost(); after_the_loss(); next(); }
    void next() { drawing(); }

    void run()
    {
        HostLossScenario scenario(kmt_, report_, search_);
        scenario.run();
        report_.finish();
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
    }

private:
    MockKmt          kmt_;
    AdapterSearch    search_;
    ScriptedHost     host_;
    av::MockPlatform report_memory_;
    GuestReport      report_;
};

}

TEST(host_loss_scenario, a_device_lost_on_the_host_removes_every_device_of_the_context_and_no_other)
{ Rig rig;
  rig.drawing();
  rig.idle();
  rig.other_before();
  rig.lost();
  rig.after_the_loss();
  rig.next();
  rig.run();
  CHECK(has_line(rig.report(), "ok host_loss.drawn\n"));
  CHECK(has_line(rig.report(), "ok host_loss.idle_made\n"));
  CHECK(has_line(rig.report(), "ok host_loss.other_before\n"));
  CHECK(has_line(rig.report(), "ok host_loss.lose\n"));
  CHECK(has_line(rig.report(), "ok host_loss.drawing_removed "));
  CHECK(has_line(rig.report(), "ok host_loss.idle_removed "));
  CHECK(has_line(rig.report(), "ok host_loss.idle_code 7\n"));
  CHECK(has_line(rig.report(), "ok host_loss.idle_refused"));
  CHECK(has_line(rig.report(), "ok host_loss.other_kept\n"));
  CHECK(has_line(rig.report(), "ok host_loss.recreated\n"));
  CHECK_EQ(rig.report().failed(), 0u); }

TEST(host_loss_scenario, without_an_avgpu_adapter_it_opens_nothing_and_says_nothing)
{ MockKmt kmt;
  AdapterSearch search(kmt);
  av::MockPlatform report_memory;
  GuestReport report(report_memory);
  HostLossScenario scenario(kmt, report, search);
  scenario.run();
  CHECK(!has_line(report, "host_loss.drawn"));
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(host_loss_scenario, a_device_the_host_refuses_to_make_or_finish_fails_the_first_draw)
{ { Rig rig;
    rig.drawing(1u);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.drawn")); }
  { Rig rig;
    rig.drawing(0u, 1u);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.drawn")); }
  { Rig rig;
    rig.drawing(0u, 0u, 1u);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.drawn")); }
  for (std::uint32_t sending = 2u; sending <= 6u; sending++) {
      Rig rig;
      rig.drawing();
      rig.kmt().fail_at(MockKmt::Call::submit, sending, MockKmt::invalid_parameter);
      rig.run();
      CHECK(has_line(rig.report(), "FAIL host_loss.drawn"));
      CHECK(!has_line(rig.report(), "host_loss.idle_made"));
  } }

TEST(host_loss_scenario, an_idle_device_or_another_context_that_cannot_be_made_ends_the_run_before_the_loss)
{ { Rig rig;
    rig.drawing();
    rig.kmt().fail_at(MockKmt::Call::create_allocation, 3u, MockKmt::invalid_parameter);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.idle_made"));
    CHECK(!has_line(rig.report(), "host_loss.lose")); }
  { Rig rig;
    rig.drawing();
    rig.idle();
    rig.kmt().fail_at(MockKmt::Call::create_device, 2u, MockKmt::invalid_parameter);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.other_before"));
    CHECK(!has_line(rig.report(), "host_loss.lose")); }
  { Rig rig;
    rig.drawing();
    rig.idle();
    rig.kmt().fail_at(MockKmt::Call::create_allocation, 5u, MockKmt::invalid_parameter);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.other_before"));
    CHECK(!has_line(rig.report(), "host_loss.lose")); } }

TEST(host_loss_scenario, a_host_that_keeps_its_devices_is_a_note_and_one_that_answers_oddly_is_a_failure)
{ { Rig rig;
    rig.drawing();
    rig.idle();
    rig.other_before();
    rig.host().host_loss(CommandHeader::host_device_kept);
    rig.run();
    CHECK(has_line(rig.report(), "note host_loss.lose "));
    CHECK(!has_line(rig.report(), "host_loss.drawing_removed"));
    CHECK_EQ(rig.report().failed(), 0u); }
  { Rig rig;
    rig.drawing();
    rig.idle();
    rig.other_before();
    rig.host().host_loss(2u);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.lose"));
    CHECK(!has_line(rig.report(), "host_loss.drawing_removed")); } }

TEST(host_loss_scenario, a_drawing_context_that_still_finishes_after_the_loss_has_not_lost_its_device)
{ Rig rig;
  rig.drawing();
  rig.idle();
  rig.other_before();
  rig.lost();
  rig.after_the_loss(true);
  rig.next();
  rig.run();
  CHECK(has_line(rig.report(), "FAIL host_loss.drawing_removed"));
  CHECK(has_line(rig.report(), "ok host_loss.idle_removed ")); }

TEST(host_loss_scenario, an_idle_context_the_host_never_faults_is_given_up_on_after_its_seconds)
{ Rig rig(false);
  rig.kmt().set_frequency(1000u);
  rig.kmt().reads_clock(nullptr, 100u);
  rig.everything();
  rig.run();
  CHECK(has_line(rig.report(), "FAIL host_loss.idle_removed "));
  CHECK(has_line(rig.report(), "FAIL host_loss.idle_code status=7 0\n")); }

TEST(host_loss_scenario, a_clock_without_a_frequency_measures_no_time_and_one_far_ahead_the_most_it_can)
{ { Rig rig;
    rig.kmt().set_frequency(0u);
    rig.everything();
    rig.run();
    CHECK(has_line(rig.report(), "ok host_loss.drawing_removed 0\n"));
    CHECK(has_line(rig.report(), "ok host_loss.idle_removed 0\n")); }
  { Rig rig;
    rig.kmt().set_frequency(1000u);
    rig.kmt().reads_clock(nullptr, 10000000000000ull);
    rig.everything();
    rig.run();
    CHECK(has_line(rig.report(), "ok host_loss.drawing_removed 4294967295\n")); } }

TEST(host_loss_scenario, a_context_that_cannot_be_made_again_after_the_loss_fails_the_recreation)
{ { Rig rig;
    rig.everything();
    rig.kmt().fail_at(MockKmt::Call::create_device, 3u, MockKmt::invalid_parameter);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.recreated")); }
  { Rig rig;
    rig.everything();
    rig.kmt().fail_at(MockKmt::Call::create_allocation, 7u, MockKmt::invalid_parameter);
    rig.run();
    CHECK(has_line(rig.report(), "FAIL host_loss.recreated")); } }
