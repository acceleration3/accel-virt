#ifndef AV_PITCHED_EXTENT_HPP
#define AV_PITCHED_EXTENT_HPP

#include <stdint.h>

namespace av {

class PitchedExtent {
public:
    PitchedExtent(uint64_t line, uint64_t rows, uint64_t slices, uint64_t row_pitch, uint64_t slice_pitch);

    bool empty() const;
    bool pitches_hold() const;
    [[nodiscard]] bool offset_of(uint64_t row, uint64_t slice, uint64_t &offset) const;
    [[nodiscard]] bool end(uint64_t start, uint64_t &end) const;
    bool within(uint64_t size) const;
    bool fits(uint64_t size) const;

private:
    uint64_t line_;
    uint64_t rows_;
    uint64_t slices_;
    uint64_t row_pitch_;
    uint64_t slice_pitch_;
};

}

#endif
