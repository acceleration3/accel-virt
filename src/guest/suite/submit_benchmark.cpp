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

}

SubmitBenchmark::SubmitBenchmark(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), report_(report), steps_(kmt, report, "submit"), link_(kmt, search, steps_, SubApi::vulkan),
      doorbell_ns_(0u), kernel_ns_(0u), measured_(false), trip_ns_{}
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
    uint32_t answered = 0u;
    if (steps_.check("warm", link_.transport().nop(answered)) != Status::ok) return;

    measured_ = link_.transport().submissions_through_doorbell() != 0u;
    report_.note("submit", "doorbell", measured_ ? "connected" : "none");
    if (measured_ && steps_.check("doorbell", time_path(doorbell_ns_)) != Status::ok) return;
    link_.transport().give_back_doorbell();
    if (steps_.check("kernel", time_path(kernel_ns_)) != Status::ok) return;

    steps_.check_measured("doorbell_ns", Status::ok, doorbell_ns_);
    steps_.check_measured("kernel_ns", Status::ok, kernel_ns_);
    steps_.check_measured("ratio", Status::ok, ratio());
}

void SubmitBenchmark::round_trips()
{
    AV_SCOPE("SubmitBenchmark::round_trips");
    constexpr uint32_t percent = 100u;
    constexpr uint32_t tail_percent = 99u;
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    uint32_t answered = 0u;
    if (steps_.check("warm", link_.transport().nop(answered)) != Status::ok) return;
    uint64_t frequency = 0u;
    uint64_t total = 0u;
    for (uint32_t trip = 0u; trip < trips; trip++) {
        const uint64_t began = kmt_.performance_counter(frequency);
        const Status sent = link_.transport().nop(answered);
        if (sent != Status::ok) {
            static_cast<void>(steps_.check("nop", sent));
            return;
        }
        const uint64_t ticks = kmt_.performance_counter(frequency) - began;
        if (frequency == 0u) return;
        trip_ns_[trip] = static_cast<uint32_t>(ticks * nanoseconds_per_second / frequency);
        total += trip_ns_[trip];
    }
    for (uint32_t index = 1u; index < trips; index++) {
        const uint32_t value = trip_ns_[index];
        uint32_t       at = index;
        for (; at > 0u && trip_ns_[at - 1u] > value; at--) trip_ns_[at] = trip_ns_[at - 1u];
        trip_ns_[at] = value;
    }
    steps_.check_measured("trip_min_ns", Status::ok, trip_ns_[0]);
    constexpr uint32_t quartile = 25u, tenth = 10u, ninetieth = 90u;
    steps_.check_measured("trip_p10_ns", Status::ok, trip_ns_[trips * tenth / percent]);
    steps_.check_measured("trip_p25_ns", Status::ok, trip_ns_[trips * quartile / percent]);
    steps_.check_measured("trip_median_ns", Status::ok, trip_ns_[trips / 2u]);
    steps_.check_measured("trip_p75_ns", Status::ok, trip_ns_[trips * (percent - quartile) / percent]);
    steps_.check_measured("trip_p90_ns", Status::ok, trip_ns_[trips * ninetieth / percent]);
    steps_.check_measured("trip_p99_ns", Status::ok, trip_ns_[trips * tail_percent / percent]);
    steps_.check_measured("trip_mean_ns", Status::ok, static_cast<uint32_t>(total / trips));
    constexpr uint32_t reads = 100000u;
    const uint64_t     clock_began = kmt_.performance_counter(frequency);
    for (uint32_t read = 0u; read < reads; read++) static_cast<void>(kmt_.performance_counter(frequency));
    const uint64_t clock_ticks = kmt_.performance_counter(frequency) - clock_began;
    steps_.check_measured("clock_read_ns", Status::ok,
                          static_cast<uint32_t>(clock_ticks * nanoseconds_per_second / frequency / reads));
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
    if (!measured_) return 0u;
    return kernel_ns_ * hundredths / (doorbell_ns_ + 1u);
}

}
