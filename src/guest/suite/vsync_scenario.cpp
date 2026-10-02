#include "guest/suite/vsync_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

VsyncScenario::VsyncScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "vsync"), blanks_(0u), first_tick_(0u), last_tick_(0u),
      shortest_(0u), frequency_(0u)
      {
          AV_SCOPE("VsyncScenario::VsyncScenario");
      }

void VsyncScenario::run()
{
    AV_SCOPE("VsyncScenario::run");
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession session(kmt_, search_.adapter());
    Status       status = steps_.check("device", session.open_device());
    if (status == Status::ok) status = steps_.check("blanks", count(session));
    if (status == Status::ok)
        static_cast<void>(steps_.check_measured("rate", paced() ? Status::ok : Status::conflict,
                                                rate_centihertz()));
    /* What this process caught, which a missed pulse lowers: reported, never
     * judged, so a busy guest does not fail the driver. */
    if (status == Status::ok) static_cast<void>(steps_.check_measured("caught", Status::ok, caught_centihertz()));
    steps_.check_teardown(session);
}

/* The wait that opens the window may have begun anywhere inside a frame, so the
 * blank it returns is where the clock starts and is not counted. */
Status VsyncScenario::count(GuestSession &session)
{
    AV_SCOPE("VsyncScenario::count");
    Status status = session.await_vertical_blank(source);
    if (status != Status::ok) return status;
    first_tick_ = kmt_.performance_counter(frequency_);
    uint64_t previous = first_tick_;
    while (blanks_ < counted_blanks) {
        status = session.await_vertical_blank(source);
        if (status != Status::ok) break;
        const uint64_t now = kmt_.performance_counter(frequency_);
        const uint64_t interval = now - previous;
        if (interval != 0u && (shortest_ == 0u || interval < shortest_)) shortest_ = interval;
        previous = now;
        blanks_++;
    }
    last_tick_ = previous;
    return status;
}

uint32_t VsyncScenario::rate_centihertz() const
{
    AV_SCOPE("VsyncScenario::rate_centihertz");
    if (shortest_ == 0u) return 0u;
    return static_cast<uint32_t>(frequency_ * 100u / shortest_);
}

uint32_t VsyncScenario::caught_centihertz() const
{
    AV_SCOPE("VsyncScenario::caught_centihertz");
    const uint64_t elapsed = last_tick_ - first_tick_;
    if (blanks_ == 0u || elapsed == 0u) return 0u;
    return static_cast<uint32_t>(uint64_t{blanks_} * frequency_ * 100u / elapsed);
}

bool VsyncScenario::paced() const
{
    AV_SCOPE("VsyncScenario::paced");
    const uint32_t rate = rate_centihertz();
    const uint32_t fastest = expected_hz * 100u;
    const uint32_t slowest = fastest / 2u;
    return rate + fastest * tolerance_percent / 100u >= slowest &&
           rate <= fastest + fastest * tolerance_percent / 100u;
}

}  // namespace av
