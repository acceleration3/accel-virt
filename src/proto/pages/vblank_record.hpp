#ifndef AV_VBLANK_RECORD_HPP
#define AV_VBLANK_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VblankRecord {
public:
    static constexpr size_t   generation_at = 0u;
    static constexpr size_t   fields_at = 4u;
    static constexpr size_t   encoded_size = 28u;
    static constexpr size_t   interrupt_every_at = 32u;
    static constexpr uint32_t least_millihz = 1000u;
    static constexpr uint32_t most_millihz = 1000000u;
    static constexpr uint64_t nanoseconds_times_millihz = 1000000000000u;
    static constexpr uint32_t microhz_per_millihz = 1000u;
    static constexpr uint64_t nanoseconds_times_microhz = 1000000000000000u;

    VblankRecord();
    VblankRecord(uint32_t latched, uint64_t sequence, uint64_t vblank_ns, uint32_t refresh_millihz);

    uint32_t latched() const;
    uint64_t sequence() const;
    uint64_t vblank_ns() const;
    uint32_t period_ns() const;
    uint32_t refresh_millihz() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint32_t latched_;
    uint64_t sequence_;
    uint64_t vblank_ns_;
    uint32_t period_ns_;
    uint32_t refresh_millihz_;
};

}

#endif
