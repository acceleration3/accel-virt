#include "guest/suite/transport_scenario.hpp"

#include "core/support/scope_exit.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

namespace {

constexpr uint64_t microseconds_per_second = 1000000u;

}  // namespace

TransportScenario::TransportScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search, SubApi sub_api,
                                     const char *name)
    : kmt_(kmt), steps_(kmt, report, name), link_(kmt, search, steps_, sub_api) {}

void TransportScenario::run()
{
    ScopeExit closing([&] { link_.close(); });
    if (link_.open() != Status::ok) return;
    if (first_round_trip() != Status::ok) return;
    warm_round_trips();
}

Status TransportScenario::timed_nop(uint32_t &answered, uint32_t &microseconds)
{
    uint64_t       frequency = 0u;
    const uint64_t sent_at = kmt_.performance_counter(frequency);
    /* A failed call has lost the context, which is the state a device reset
     * leaves it in; an answer with anything in it is not a no-op's. */
    const Status   sent = link_.transport().nop(answered);
    const uint64_t elapsed = kmt_.performance_counter(frequency) - sent_at;
    microseconds = frequency == 0u ? 0u : static_cast<uint32_t>(elapsed * microseconds_per_second / frequency);
    return sent;
}

Status TransportScenario::first_round_trip()
{
    uint32_t     answered = 0u;
    uint32_t     microseconds = 0u;
    const Status sent = timed_nop(answered, microseconds);
    if (steps_.check_measured("nop", sent, answered) != Status::ok) return sent;
    /* Reported, never judged: SC-002's threshold comes from a live run. */
    return steps_.check_measured("latency", Status::ok, microseconds);
}

void TransportScenario::warm_round_trips()
{
    uint32_t samples[warm_rounds] = {};
    for (uint32_t round = 0u; round < warm_rounds; round++) {
        uint32_t     answered = 0u;
        const Status sent = timed_nop(answered, samples[round]);
        if (sent != Status::ok) return static_cast<void>(steps_.check_measured("warm_nop", sent, answered));
    }
    static_cast<void>(steps_.check_measured("warm_nop", Status::ok, warm_rounds));
    for (uint32_t next = 1u; next < warm_rounds; next++) {
        const uint32_t sample = samples[next];
        uint32_t       at = next;
        for (; at != 0u && samples[at - 1u] > sample; at--) samples[at] = samples[at - 1u];
        samples[at] = sample;
    }
    static_cast<void>(steps_.check_measured("warm_median", Status::ok, samples[warm_rounds / 2u]));
    static_cast<void>(steps_.check_measured("warm_fastest", Status::ok, samples[0]));
}

}  // namespace av
