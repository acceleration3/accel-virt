#include "guest/probe/vblank_clock.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

VblankClock::VblankClock(Kmt &kmt) : kmt_(kmt)
{
    AV_SCOPE("VblankClock::VblankClock");
}

uint64_t VblankClock::now() const
{
    AV_SCOPE("VblankClock::now");
    uint64_t frequency = 0u;
    return kmt_.performance_counter(frequency);
}

uint64_t VblankClock::period()
{
    AV_SCOPE("VblankClock::period");
    AdapterSearch search(kmt_);
    if (search.find_avgpu(0u) != Status::ok) return 0u;
    uint32_t device = 0u;
    uint64_t ticks = 0u;
    if (kmt_.create_device(search.adapter(), device) == Status::ok) {
        if (kmt_.wait_for_vertical_blank(search.adapter(), device, 0u) == Status::ok) {
            const uint64_t started = now();
            uint32_t       timed = 0u;
            while (timed < timed_blanks && kmt_.wait_for_vertical_blank(search.adapter(), device, 0u) == Status::ok)
                timed++;
            if (timed == timed_blanks) ticks = (now() - started) / timed_blanks;
        }
        static_cast<void>(kmt_.destroy_device(device));
    }
    static_cast<void>(search.close());
    return ticks;
}

}  // namespace av
