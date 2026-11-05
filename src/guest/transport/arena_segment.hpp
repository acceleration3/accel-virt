#ifndef AV_ARENA_SEGMENT_HPP
#define AV_ARENA_SEGMENT_HPP

#include "core/support/status.hpp"
#include "guest/transport/host_allocation.hpp"

#include <stdint.h>

namespace av {

class ArenaSlots;
class GuestConnection;
class KmtTransport;
struct ArenaSegmentProbe;

class ArenaSegment {
public:
    static constexpr uint64_t page_bytes = 4096u;

    ArenaSegment(GuestConnection &connection, KmtTransport &transport, ArenaSlots &slots);
    ~ArenaSegment();
    ArenaSegment(const ArenaSegment &) = delete;
    ArenaSegment &operator=(const ArenaSegment &) = delete;

    [[nodiscard]] Status open(uint64_t bytes);
    [[nodiscard]] Status open_at(uint64_t base, uint64_t bytes);
    [[nodiscard]] Status close();
    [[nodiscard]] Status abandon();

    bool     held() const;
    uint64_t base() const;
    uint64_t bytes() const;
    uint8_t *view() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status make(uint64_t base, uint64_t bytes);

    ArenaSlots    &slots_;
    HostAllocation memory_;
    uint64_t       base_;
    uint64_t       bytes_;

    friend struct ArenaSegmentProbe;
};

}

#endif
