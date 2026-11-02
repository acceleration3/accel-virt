#ifndef AV_EVENT_RING_HPP
#define AV_EVENT_RING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class EventRing {
public:
    static constexpr uint32_t magic = 0x45564B52u;
    static constexpr uint32_t version = 1u;
    static constexpr size_t   header_bytes = 32u;
    static constexpr size_t   event_bytes = 16u;
    static constexpr size_t   slot_header_bytes = 16u;

    EventRing();
    EventRing(const EventRing &) = delete;
    EventRing &operator=(const EventRing &) = delete;

    [[nodiscard]] Status initialize(uint8_t *region, size_t bytes, uint32_t slots, uint32_t events_per_slot);
    bool                 recording() const;
    void                 describe_clock(uint64_t ticks_per_second);
    void                 record(uint32_t slot, uint64_t time, uint32_t name, uint32_t thread, bool begin);

private:
    uint8_t *region_;
    uint32_t slots_;
    uint32_t events_per_slot_;
};

}

#endif
