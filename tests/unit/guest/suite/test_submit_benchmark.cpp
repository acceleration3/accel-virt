#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/submit_benchmark.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::GuestReport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubmitBenchmark;

namespace {

const PciIdentity av(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::uint64_t submit_ticks = 20u;
constexpr std::uint32_t submit_ns = 2000u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

class Rig {
public:
    Rig() : search_(kmt_)
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
        kmt_.answer_waits(nullptr, 0u);
        kmt_.spend_on_submit(submit_ticks);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }
    void           close()
    {
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
    }

private:
    MockKmt       kmt_;
    AdapterSearch search_;
};

}

TEST(submit_benchmark, measures_both_paths_on_one_run_and_answers_their_ratio)
{ Rig rig;
  rig.kmt().allow_hardware_scheduling();
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK_EQ(report.failed(), 0u);

  CHECK_EQ(benchmark.doorbell_ns(), 0u);
  CHECK_EQ(benchmark.kernel_ns(), submit_ns);
  CHECK_EQ(benchmark.ratio(), submit_ns * 100u);
  CHECK(has_line(report, "note submit.doorbell connected\n"));
  CHECK(has_line(report, "ok submit.doorbell_ns 0\n"));
  CHECK(has_line(report, "ok submit.kernel_ns 2000\n"));
  CHECK(has_line(report, "ok submit.ratio 200000\n"));

  const std::uint32_t timed = SubmitBenchmark::rounds * SubmitBenchmark::block;
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit_to_hw_queue), timed + SubmitBenchmark::rounds);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(report.invariants_hold());
  rig.close(); }

TEST(submit_benchmark, a_machine_without_hardware_queues_reports_no_gain)
{ Rig rig;
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK_EQ(report.failed(), 0u);
  CHECK_EQ(benchmark.doorbell_ns(), 0u);
  CHECK_EQ(benchmark.kernel_ns(), submit_ns);
  CHECK_EQ(benchmark.ratio(), 0u);
  CHECK(has_line(report, "note submit.doorbell none\n"));
  CHECK(has_line(report, "ok submit.ratio 0\n"));
  rig.close(); }

TEST(submit_benchmark, a_run_that_cannot_be_measured_fails_rather_than_reporting_a_number)
{ { Rig rig;
    rig.kmt().set_frequency(0u);
    av::MockPlatform report_memory;
    GuestReport     report(report_memory);
    SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
    benchmark.run();
    CHECK(has_line(report, "FAIL submit.kernel status=6"));
    CHECK(!has_line(report, "submit.ratio"));
    CHECK_EQ(benchmark.kernel_ns(), 0u);
    rig.close(); }

  Rig rig;
  rig.kmt().fail_at(MockKmt::Call::submit, 2u, 0xC0000001u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK(has_line(report, "FAIL submit.kernel status=7"));
  CHECK(!has_line(report, "submit.ratio"));
  rig.close(); }


TEST(submit_benchmark, a_step_that_fails_stops_the_run_where_it_failed)
{ { Rig rig;
    rig.kmt().fail_at(MockKmt::Call::submit, 1u, 0xC0000001u);
    av::MockPlatform report_memory;
    GuestReport     report(report_memory);
    SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
    benchmark.run();
    CHECK(has_line(report, "FAIL submit.warm status=7"));
    CHECK(!has_line(report, "submit.kernel"));
    CHECK_EQ(benchmark.ratio(), 0u);
    rig.close(); }

  Rig rig;
  rig.kmt().allow_hardware_scheduling();
  rig.kmt().fail_at(MockKmt::Call::wait_for_hw_queue, 2u, 0xC0000001u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK(has_line(report, "FAIL submit.doorbell status=7"));
  CHECK(!has_line(report, "submit.kernel_ns"));
  CHECK_EQ(benchmark.doorbell_ns(), 0u);
  CHECK_EQ(benchmark.kernel_ns(), 0u);
  rig.close(); }

TEST(submit_benchmark, round_trips_report_their_spread_in_order)
{ Rig rig;
  rig.kmt().spend_on_wait(10u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.round_trips();
  CHECK_EQ(report.failed(), 0u);
  CHECK(has_line(report, "ok submit.warm\n"));
  const char *const steps[] = {"trip_min_ns", "trip_p10_ns", "trip_p25_ns", "trip_median_ns",
                               "trip_p75_ns", "trip_p90_ns", "trip_p99_ns", "trip_mean_ns", "clock_read_ns"};
  for (const char *step : steps) CHECK(has_line(report, step));
  CHECK(!has_line(report, "ok submit.trip_min_ns 0\n"));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), SubmitBenchmark::trips + 1u);
  rig.close(); }

TEST(submit_benchmark, round_trips_stop_at_the_first_no_op_that_fails)
{ Rig rig;
  rig.kmt().fail_at(MockKmt::Call::wait_for_context, 3u, 0xC0000001u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.round_trips();
  CHECK(has_line(report, "ok submit.warm\n"));
  CHECK(has_line(report, "FAIL submit.nop"));
  CHECK(!has_line(report, "trip_min_ns"));
  CHECK_EQ(report.failed(), 1u);
  rig.close();

  Rig cold;
  cold.kmt().fail_at(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  av::MockPlatform refused_memory;
  GuestReport     refused(refused_memory);
  SubmitBenchmark again(cold.kmt(), refused, cold.search());
  again.round_trips();
  CHECK(has_line(refused, "FAIL submit.warm"));
  CHECK(!has_line(refused, "submit.nop"));
  cold.close(); }

TEST(submit_benchmark, round_trips_alternating_slow_and_fast_are_reported_in_order)
{ Rig rig;
  rig.kmt().spend_on_wait(10u);
  rig.kmt().slow_every(2u, 1000u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.round_trips();
  CHECK_EQ(report.failed(), 0u);
  CHECK(has_line(report, "ok submit.trip_min_ns "));
  CHECK(has_line(report, "ok submit.trip_p99_ns "));
  CHECK(!has_line(report, "ok submit.trip_min_ns 0\n"));
  rig.close(); }

namespace {

void add_avgpu(MockKmt &kmt) { kmt.add_adapter(av, AdapterScenario::wddm_3_2); }

}

TEST(submit_benchmark, round_trips_without_an_adapter_to_open_report_nothing)
{ MockKmt kmt;
  add_avgpu(kmt);
  AdapterSearch search(kmt);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(kmt, report, search);
  benchmark.round_trips();
  CHECK(!has_line(report, "submit.warm"));
  CHECK(!has_line(report, "trip_min_ns"));
  CHECK_EQ(kmt.open_objects(), 0u); }

TEST(submit_benchmark, a_clock_that_counts_no_ticks_a_second_ends_the_round_trips_unmeasured)
{ Rig rig;
  rig.kmt().set_frequency(0u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.round_trips();
  CHECK(has_line(report, "ok submit.warm\n"));
  CHECK(!has_line(report, "trip_min_ns"));
  CHECK_EQ(report.failed(), 0u);
  rig.close(); }
