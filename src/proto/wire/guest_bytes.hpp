#ifndef AV_GUEST_BYTES_HPP
#define AV_GUEST_BYTES_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestBytes {
public:
    static constexpr uint32_t slot_shift = 26u;
    static constexpr uint64_t slot_bytes = uint64_t{1} << slot_shift;
    static constexpr uint32_t slot_count = 64u;

    GuestBytes();
    GuestBytes(const GuestBytes &) = delete;
    GuestBytes &operator=(const GuestBytes &) = delete;

    [[nodiscard]] Status bind(uint64_t base, uint8_t *memory, uint64_t length);
    [[nodiscard]] Status unbind(const uint8_t *memory);
    [[nodiscard]] Status resolve(uint64_t offset, uint64_t bytes, uint8_t *&out) const;
    uint8_t             *at(uint64_t offset, uint64_t bytes) const;

    bool     holds(const void *bytes) const;
    uint64_t segment_bytes(uint32_t slot) const;
    uint64_t base_of(const uint8_t *memory) const;
    uint64_t base_at(uint64_t offset) const;
    uint32_t segments() const;

    bool invariants_hold() const;

    static constexpr uint64_t no_base = ~uint64_t{0};

private:
    struct Slot {
        uint8_t *memory_;
        uint64_t length_;
        uint32_t first_;
    };

    uint32_t slots_for(uint64_t length) const;

    Slot     slots_[slot_count];
    uint32_t segments_;
};

}

#endif
