/* Every scope a thread enters and leaves, with its time, for a timeline
 * viewer: tools/perfetto_trace.py turns a region of these into a trace
 * ui.perfetto.dev opens. A slot of the trace table owns one ring, so its
 * thread is the only writer and nothing is atomic; a ring keeps the newest
 * events_per_slot and overwrites the oldest. Off unless a region is given. */
#ifndef AV_EVENT_RING_HPP
#define AV_EVENT_RING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class EventRing {
public:
    static constexpr uint32_t magic = 0x45564B52u;   /* "RKVE" */
    static constexpr uint32_t version = 1u;
    static constexpr size_t   header_bytes = 32u;
    static constexpr size_t   event_bytes = 16u;
    static constexpr size_t   slot_header_bytes = 16u;

    EventRing();
    EventRing(const EventRing &) = delete;
    EventRing &operator=(const EventRing &) = delete;

    static size_t bytes_for(uint32_t slots, uint32_t events_per_slot);
    /* The region must hold bytes_for(slots, events_per_slot). */
    [[nodiscard]] Status initialize(uint8_t *region, size_t bytes, uint32_t slots, uint32_t events_per_slot);
    bool                 recording() const;
    /* The stamps' unit, in the header's bytes 16..23, for a reader. */
    void                 describe_clock(uint64_t ticks_per_second);
    /* `time` in 100 ns units, the trace host's clock. */
    void                 record(uint32_t slot, uint64_t time, uint32_t name, uint32_t thread, bool begin);

private:
    uint8_t *region_;
    uint32_t slots_;
    uint32_t events_per_slot_;
};

}  // namespace av

#endif  // AV_EVENT_RING_HPP
