/* What the device says of one head's latest vertical blank (docs/protocol.md,
 * "Display clock"): the blank's number, the frame it put on screen, when, and
 * the rate the head runs at. These are the fields of a head record between its
 * generation and the guest's own interrupt_every; the page accessors on each
 * side own the generation that makes a read of them whole. */
#ifndef AV_VBLANK_RECORD_HPP
#define AV_VBLANK_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VblankRecord {
public:
    /* Offsets within a head record. */
    static constexpr size_t   generation_at = 0u;
    static constexpr size_t   fields_at = 4u;
    static constexpr size_t   encoded_size = 28u;
    static constexpr size_t   interrupt_every_at = 32u;
    /* Rates the protocol allows, as DISPLAY_TIMING's. */
    static constexpr uint32_t least_millihz = 1000u;
    static constexpr uint32_t most_millihz = 1000000u;
    /* Nanoseconds in a second, times the thousand a millihertz divides by. */
    static constexpr uint64_t nanoseconds_times_millihz = 1000000000000u;
    /* The same for the schedule's finer rate, in microhertz. */
    static constexpr uint32_t microhz_per_millihz = 1000u;
    static constexpr uint64_t nanoseconds_times_microhz = 1000000000000000u;

    VblankRecord();
    /* The period is the rate's, rounded down to the nanosecond. */
    VblankRecord(uint32_t latched, uint64_t sequence, uint64_t vblank_ns, uint32_t refresh_millihz);

    uint32_t latched() const;
    uint64_t sequence() const;
    uint64_t vblank_ns() const;
    uint32_t period_ns() const;
    uint32_t refresh_millihz() const;

    /* too_large for less room than the fields. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for a rate out of range or a period that is not the rate's.
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint32_t latched_;
    uint64_t sequence_;
    uint64_t vblank_ns_;
    uint32_t period_ns_;
    uint32_t refresh_millihz_;
};

}  // namespace av

#endif  // AV_VBLANK_RECORD_HPP
