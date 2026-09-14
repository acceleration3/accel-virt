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
    uint32_t       answered = 0u;
    uint64_t       frequency = 0u;
    const uint64_t sent_at = kmt_.performance_counter(frequency);
    /* A failed call has lost the context, which is the state a device reset
     * leaves it in; an answer with anything in it is not a no-op's. */
    const Status   nop = link_.transport().nop(answered);
    const uint64_t answered_at = kmt_.performance_counter(frequency);
    if (steps_.check_measured("nop", nop, answered) != Status::ok) return;
    const uint64_t elapsed = answered_at - sent_at;
    const uint64_t microseconds = frequency == 0u ? 0u : elapsed * microseconds_per_second / frequency;
    /* Reported, never judged: SC-002's threshold comes from a live run. */
    static_cast<void>(steps_.check_measured("latency", Status::ok, static_cast<uint32_t>(microseconds)));
}

}  // namespace av
