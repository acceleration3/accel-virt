#ifndef AV_CLOCK_ORIGIN_RECORD_HPP
#define AV_CLOCK_ORIGIN_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClockOriginRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00435641u;
    static constexpr uint64_t most_frequency = 4000000000ull;

    ClockOriginRecord();
    ClockOriginRecord(uint64_t counter, uint64_t ticks, uint64_t frequency);

    uint64_t counter() const;
    uint64_t ticks() const;
    uint64_t frequency() const;
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    uint64_t counter_;
    uint64_t ticks_;
    uint64_t frequency_;
};

}

#endif
