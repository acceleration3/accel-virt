/* How many bytes an event ring of a given shape takes: the region a writer
 * maps and the ring checks it was given agree through this. */
#ifndef AV_EVENT_RING_LAYOUT_HPP
#define AV_EVENT_RING_LAYOUT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class EventRingLayout {
public:
    EventRingLayout(uint32_t slots, uint32_t events_per_slot);

    size_t bytes() const;

private:
    uint32_t slots_;
    uint32_t events_per_slot_;
};

}  // namespace av

#endif  // AV_EVENT_RING_LAYOUT_HPP
