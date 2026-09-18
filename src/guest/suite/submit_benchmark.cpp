#include "guest/suite/submit_benchmark.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/suite/guest_report.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {

constexpr uint64_t nanoseconds_per_second = 1000000000ull;
constexpr uint32_t hundredths = 100u;

}  // namespace

SubmitBenchmark::SubmitBenchmark(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), report_(report), steps_(kmt, report, "submit"), link_(kmt, search, steps_, SubApi::vulkan),
      doorbell_ns_(0u), kernel_ns_(0u), measured_(false)
{
    AV_SCOPE("SubmitBenchmark::SubmitBenchmark");
}

Status SubmitBenchmark::time_block(uint64_t &elapsed_ticks)
{
    AV_SCOPE("SubmitBenchmark::time_block");
    uint64_t       frequency = 0u;
    const uint64_t began = kmt_.performance_counter(frequency);
    for (uint32_t sent = 0u; sent < block; sent++) {
        WireWriter            request = link_.transport().request_writer();
        const TransportResult queued = link_.transport().send_async(CommandHeader::nop_id, request);
        if (queued != TransportResult::ok) return Status::invalid_state;
    }
    elapsed_ticks += kmt_.performance_counter(frequency) - began;
    /* Outside the timing: the device catches up, so the next block starts on a
     * ring with room in it rather than measuring how fast the host is. */
    uint32_t answered = 0u;
    return link_.transport().nop(answered);
}

Status SubmitBenchmark::time_path(uint32_t &per_submit_ns)
{
    AV_SCOPE("SubmitBenchmark::time_path");
    per_submit_ns = 0u;
    uint64_t elapsed = 0u;
    for (uint32_t round = 0u; round < rounds; round++) {
        const Status block_sent = time_block(elapsed);
        if (block_sent != Status::ok) return block_sent;
    }
    uint64_t frequency = 0u;
    static_cast<void>(kmt_.performance_counter(frequency));
    if (frequency == 0u) return Status::not_supported;
    per_submit_ns = static_cast<uint32_t>(elapsed * nanoseconds_per_second / frequency / (rounds * block));
    return Status::ok;
}

void SubmitBenchmark::run()
{
    AV_SCOPE("SubmitBenchmark::run");
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    /* The first round trip opens the host's backend, which is not a submission
     * cost and must not land inside a timed block. */
    uint32_t answered = 0u;
    if (steps_.check("warm", link_.transport().nop(answered)) != Status::ok) return;

    measured_ = link_.transport().submissions_through_doorbell() != 0u;
    /* A machine whose OS hands out no hardware queue has no gain to report,
     * and the run says which it was (research.md §11). */
    report_.note("submit", "doorbell", measured_ ? "connected" : "none");
    if (measured_ && steps_.check("doorbell", time_path(doorbell_ns_)) != Status::ok) return;
    /* Both costs on one boot: the doorbell is given back and the same batches
     * go through the kernel. */
    link_.transport().give_back_doorbell();
    if (steps_.check("kernel", time_path(kernel_ns_)) != Status::ok) return;

    steps_.check_measured("doorbell_ns", Status::ok, doorbell_ns_);
    steps_.check_measured("kernel_ns", Status::ok, kernel_ns_);
    steps_.check_measured("ratio", Status::ok, ratio());
}

uint32_t SubmitBenchmark::doorbell_ns() const
{
    AV_SCOPE("SubmitBenchmark::doorbell_ns");
    return doorbell_ns_;
}

uint32_t SubmitBenchmark::kernel_ns() const
{
    AV_SCOPE("SubmitBenchmark::kernel_ns");
    return kernel_ns_;
}

uint32_t SubmitBenchmark::ratio() const
{
    AV_SCOPE("SubmitBenchmark::ratio");
    /* No doorbell is no comparison, and a doorbell submission too fast for the
     * clock is charged one nanosecond - which makes the ratio a lower bound on
     * the gain rather than a division by nothing. */
    if (!measured_) return 0u;
    if (doorbell_ns_ == 0u) return kernel_ns_ * hundredths;
    return kernel_ns_ * hundredths / doorbell_ns_;
}

}  // namespace av
