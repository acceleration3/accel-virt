#ifndef AV_BGRA_ROWS_HPP
#define AV_BGRA_ROWS_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BgraRows {
public:
    enum class Channels { unchanged, red_blue_swapped };

    BgraRows(uint8_t *base, uint64_t bytes, uint64_t pitch);
    BgraRows(const uint8_t *base, uint64_t bytes, uint64_t pitch);

    // Exact in-place conversion is supported; otherwise the spans must not overlap.
    // Both rectangles are checked before any pixel is written. Padding is left alone.
    [[nodiscard]] Status copy_from(const BgraRows &from, uint32_t width, uint32_t height, Channels channels);
    bool invariants_hold() const;

private:
    bool holds(uint32_t width, uint32_t height) const;

    uint8_t       *base_;
    const uint8_t *source_;
    uint64_t       bytes_;
    uint64_t       pitch_;
};

}  // namespace av

#endif  // AV_BGRA_ROWS_HPP
