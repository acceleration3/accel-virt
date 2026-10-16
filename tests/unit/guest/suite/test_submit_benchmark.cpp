/* What the doorbell bought, measured over MockKmt (feature 049, US3). The mock
 * charges a kernel submission the ticks a transition into dxgkrnl costs and
 * charges a doorbell store nothing - which is not a simplification but the
 * whole claim: a store is a write in the process, and this mock never sees it.
 *
 * What these tests judge is that both paths are measured on one run, that the
 * ratio is the driver's property and not the clock's, and that a machine whose
 * OS hands out no hardware queue reports no gain rather than the kernel's own
 * cost compared against nothing.
 */
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

const PciIdentity av(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);
/* Ticks per second in MockKmt's clock, and the cost of one kernel submission:
 * 20 ticks is two microseconds there. */
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

}  // namespace

TEST(submit_benchmark, measures_both_paths_on_one_run_and_answers_their_ratio)
{ Rig rig;
  rig.kmt().allow_hardware_scheduling();
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK_EQ(report.failed(), 0u);

  /* A doorbell submission costs nothing this mock can see, and a kernel one
   * costs the transition: the ratio is what the run is for. */
  CHECK_EQ(benchmark.doorbell_ns(), 0u);
  CHECK_EQ(benchmark.kernel_ns(), submit_ns);
  CHECK_EQ(benchmark.ratio(), submit_ns * 100u);
  CHECK(has_line(report, "note submit.doorbell connected\n"));
  CHECK(has_line(report, "ok submit.doorbell_ns 0\n"));
  CHECK(has_line(report, "ok submit.kernel_ns 2000\n"));
  CHECK(has_line(report, "ok submit.ratio 200000\n"));

  /* Every timed submission of the first path went through the doorbell and
   * every one of the second through the kernel: one warm no-op and one drain
   * per block are the only others. */
  /* The kernel path's timed submissions and its drains, and nothing else: the
   * doorbell path's - and the warm no-op, made while the doorbell was still
   * connected - never reached the kernel at all. They go through the queue,
   * not the context, because a hardware-scheduled context refuses
   * D3DKMTSubmitCommand outright (research.md §13). */
  const std::uint32_t timed = SubmitBenchmark::rounds * SubmitBenchmark::block;
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit_to_hw_queue), timed + SubmitBenchmark::rounds);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(report.invariants_hold());
  rig.close(); }

/* A machine whose OS hands out no hardware queue measures the only path it
 * has, and reports no gain at all rather than one it did not have. */
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

/* A clock that cannot say how long a tick is measures nothing, and a driver
 * that refuses a submission mid-run fails the step it refused. */
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
  /* The second submission of the first block: the warm no-op's is the first. */
  rig.kmt().fail_at(MockKmt::Call::submit, 2u, 0xC0000001u);
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK(has_line(report, "FAIL submit.kernel status=7"));
  CHECK(!has_line(report, "submit.ratio"));
  rig.close(); }


/* Each step's failure stops the run where it happened: a first round trip the
 * device does not answer, and a doorbell path that fails while being timed. */
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

  /* The doorbell path, timed and then refused: the first drain of the first
   * block is the second wait of the run, the warm no-op's being the first. */
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

/* Answered no-ops one at a time: every percentile of their cost, in order, and
 * what a read of the clock costs beside them. */
TEST(submit_benchmark, round_trips_report_their_spread_in_order)
{ Rig rig;
  rig.kmt().spend_on_wait(10u);   /* a microsecond a trip, after the reply-slot watch */
  av::MockPlatform report_memory;
  GuestReport     report(report_memory);
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.round_trips();
  CHECK_EQ(report.failed(), 0u);
  CHECK(has_line(report, "ok submit.warm\n"));
  const char *const steps[] = {"trip_min_ns", "trip_p10_ns", "trip_p25_ns", "trip_median_ns",
                               "trip_p75_ns", "trip_p90_ns", "trip_p99_ns", "trip_mean_ns", "clock_read_ns"};
  for (const char *step : steps) CHECK(has_line(report, step));
  /* No trip is faster than the wait it paid for. */
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
