#include "core/trace/event_ring_layout.hpp"

#include "core/trace/event_ring.hpp"

namespace av {

EventRingLayout::EventRingLayout(uint32_t slots, uint32_t events_per_slot)
    : slots_(slots), events_per_slot_(events_per_slot)
{
}

size_t EventRingLayout::bytes() const
{
    return EventRing::header_bytes +
           static_cast<size_t>(slots_) *
               (EventRing::slot_header_bytes + static_cast<size_t>(events_per_slot_) * EventRing::event_bytes);
}

}  // namespace av
