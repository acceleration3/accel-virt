#include "miniport/display/flip_log.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

FlipLog::FlipLog(Platform &platform) : platform_(platform), next_(0u), counters_(), ids_(), marks_(), extras_()
{
    AV_SCOPE("FlipLog::FlipLog");
}

void FlipLog::note(uint64_t id, uint64_t mark, uint64_t extra)
{
    AV_SCOPE("FlipLog::note");
    platform_.require_irql_at_most(Irql::isr, "FlipLog::note");
    uint64_t       frequency = 0u;
    const uint64_t at = platform_.performance_counter(frequency);
    const uint64_t next = platform_.load_atomic(next_);
    const uint32_t slot = static_cast<uint32_t>(next % capacity);
    platform_.store_atomic(counters_[slot], at);
    platform_.store_atomic(ids_[slot], id);
    platform_.store_atomic(marks_[slot], mark);
    platform_.store_atomic(extras_[slot], extra);
    platform_.store_atomic(next_, next + 1u);
}

uint64_t FlipLog::count()
{
    AV_SCOPE("FlipLog::count");
    return platform_.load_atomic(next_);
}

void FlipLog::report(StateWriter &writer, const char *name)
{
    AV_SCOPE("FlipLog::report");
    uint64_t       frequency = 0u;
    static_cast<void>(platform_.performance_counter(frequency));
    const uint64_t next = platform_.load_atomic(next_);
    const uint64_t first = next > capacity ? next - capacity : 0u;
    writer.open(name, 0u);
    writer.number("frequency", frequency);
    writer.number("count", next);
    for (uint64_t entry = first; entry < next; entry++) {
        const uint32_t slot = static_cast<uint32_t>(entry % capacity);
        writer.open("at", entry);
        writer.number("counter", platform_.load_atomic(counters_[slot]));
        writer.number("id", platform_.load_atomic(ids_[slot]));
        writer.number("mark", platform_.load_atomic(marks_[slot]));
        writer.number("extra", platform_.load_atomic(extras_[slot]));
        writer.close();
    }
    writer.close();
}

}
