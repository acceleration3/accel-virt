#include "guest/suite/fence_stress_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

namespace {

constexpr uint64_t command_buffer_bytes = 4096u;
constexpr uint64_t command_bytes = 64u;

}  // namespace

FenceStressScenario::FenceStressScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search,
                                         uint32_t seconds)
    : kmt_(kmt), search_(search), steps_(kmt, report, "stress"), seconds_(seconds), completed_(0u), blanks_(0u),
      worst_(0u), frequency_(0u)
      {
          AV_SCOPE("FenceStressScenario::FenceStressScenario");
      }

void FenceStressScenario::run()
{
    AV_SCOPE("FenceStressScenario::run");
    if (seconds_ == 0u) return;
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession           session(kmt_, search_.adapter());
    const ClientDescriptor descriptor(command_buffer_bytes);
    uint32_t               index = 0u;
    uint64_t               address = 0u;
    Status                 status = steps_.check("device", session.open_device());
    if (status == Status::ok)
        status = steps_.check("allocation", session.allocate(descriptor.bytes(), descriptor.size(), index));
    if (status == Status::ok) status = steps_.check("paging_queue", session.open_paging_queue());
    if (status == Status::ok) status = steps_.check("map", session.map(index, descriptor.pages(), address));
    if (status == Status::ok) status = steps_.check("context", session.open_context());
    if (status == Status::ok) {
        /* The run first, then what it counted: an argument list may read
         * completed_ before it calls stress(), and a live run reported zero
         * fences beside a worst wait of thirteen milliseconds because of it. */
        const Status ran = stress(session, address);
        status = steps_.check_measured("fences", ran, completed_);
        static_cast<void>(steps_.check_measured("worst_wait_us", Status::ok,
                                                static_cast<uint32_t>(worst_wait_microseconds())));
        static_cast<void>(steps_.check_measured("blanks", Status::ok, blanks_));
    }
    steps_.check_teardown(session);
}

/* One fence at a time, so a wait that never wakes stops the run where it
 * happened rather than behind a queue of others. */
Status FenceStressScenario::stress(GuestSession &session, uint64_t address)
{
    AV_SCOPE("FenceStressScenario::stress");
    const uint64_t started = kmt_.performance_counter(frequency_);
    const uint64_t deadline = started + uint64_t{seconds_} * frequency_;
    uint64_t       previous = started;
    Status         status = Status::ok;
    while (status == Status::ok && previous < deadline) {
        status = session.submit(address, command_bytes);
        if (status != Status::ok) break;
        status = session.wait();
        if (status != Status::ok) break;
        const uint64_t now = kmt_.performance_counter(frequency_);
        const uint64_t waited = now - previous;
        if (waited > worst_) worst_ = waited;
        previous = now;
        completed_++;
        if (completed_ % fences_per_blank != 0u) continue;
        /* The last waiter leaving turns the blank's interrupt off, and this
         * one arriving turns it back on. */
        status = session.await_vertical_blank(0u);
        blanks_++;
        previous = kmt_.performance_counter(frequency_);
    }
    return status;
}

uint32_t FenceStressScenario::completed() const
{
    AV_SCOPE("FenceStressScenario::completed");
    return completed_;
}

uint64_t FenceStressScenario::worst_wait_microseconds() const
{
    AV_SCOPE("FenceStressScenario::worst_wait_microseconds");
    if (frequency_ == 0u) return 0u;
    return worst_ * microseconds_per_second / frequency_;
}

}  // namespace av
