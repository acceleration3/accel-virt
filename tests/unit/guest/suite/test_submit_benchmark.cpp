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
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/submit_benchmark.hpp"
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
using av::SubmitBenchmark;

namespace {

const PciIdentity rvk(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                      AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);
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
        kmt_.add_adapter(rvk, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find(rvk), Status::ok);
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
  GuestReport     report;
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
   * connected - never reached the kernel at all. */
  const std::uint32_t timed = SubmitBenchmark::rounds * SubmitBenchmark::block;
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), timed + SubmitBenchmark::rounds);
  CHECK(report.invariants_hold());
  rig.close(); }

/* A machine whose OS hands out no hardware queue measures the only path it
 * has, and reports no gain at all rather than one it did not have. */
TEST(submit_benchmark, a_machine_without_hardware_queues_reports_no_gain)
{ Rig rig;
  GuestReport     report;
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
    GuestReport     report;
    SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
    benchmark.run();
    CHECK(has_line(report, "FAIL submit.kernel status=6"));
    CHECK(!has_line(report, "submit.ratio"));
    CHECK_EQ(benchmark.kernel_ns(), 0u);
    rig.close(); }

  Rig rig;
  /* The second submission of the first block: the warm no-op's is the first. */
  rig.kmt().fail_at(MockKmt::Call::submit, 2u, 0xC0000001u);
  GuestReport     report;
  SubmitBenchmark benchmark(rig.kmt(), report, rig.search());
  benchmark.run();
  CHECK(has_line(report, "FAIL submit.kernel status=7"));
  CHECK(!has_line(report, "submit.ratio"));
  rig.close(); }

