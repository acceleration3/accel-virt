/* What user-mode submission bought, measured on the boot it ran on (feature
 * 049, US3): the same no-op batches submitted through the process's doorbell
 * and then, with the doorbell given back, through `D3DKMTSubmitCommand` - so
 * both costs come from one machine, one host and one device.
 *
 * What the ratchet holds is their ratio, not either cost: the nanoseconds are
 * a property of the host this ran on, and the ratio is a property of the
 * driver (research.md §5). A machine whose OS hands out no hardware queue
 * measures no doorbell cost at all, and says so rather than reporting a gain
 * it did not have.
 */
#ifndef AV_SUBMIT_BENCHMARK_HPP
#define AV_SUBMIT_BENCHMARK_HPP

#include "core/support/status.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "guest/suite/scenario_transport.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class Kmt;

class SubmitBenchmark {
public:
    /* Submissions timed on each path, in blocks the ring holds without the
     * device having to catch up inside a timed one. */
    static constexpr uint32_t rounds = 32u;
    static constexpr uint32_t block = 8u;

    SubmitBenchmark(Kmt &kmt, GuestReport &report, const AdapterSearch &search);
    SubmitBenchmark(const SubmitBenchmark &) = delete;
    SubmitBenchmark &operator=(const SubmitBenchmark &) = delete;

    void run();

    /* Per submission, 0 for a path that was not measured. */
    uint32_t doorbell_ns() const;
    uint32_t kernel_ns() const;
    /* What one kernel submission costs in doorbell submissions, in hundredths:
     * 250 is two and a half times. 0 when there was no doorbell to compare. */
    uint32_t ratio() const;

private:
    /* One block of submissions with nothing else inside the timing, then a
     * synchronous no-op outside it so the device has consumed them before the
     * next block is timed. */
    [[nodiscard]] Status time_block(uint64_t &elapsed_ticks);
    /* `rounds` blocks: the per-submission cost, or the first failure. */
    [[nodiscard]] Status time_path(uint32_t &per_submit_ns);

    Kmt              &kmt_;
    GuestReport      &report_;
    ScenarioSteps     steps_;
    ScenarioTransport link_;
    uint32_t          doorbell_ns_;
    uint32_t          kernel_ns_;
    /* Whether this machine gave the transport a doorbell to measure at all. */
    bool              measured_;
};

}  // namespace av

#endif  // AV_SUBMIT_BENCHMARK_HPP
