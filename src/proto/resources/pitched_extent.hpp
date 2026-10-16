/* The bytes a box spans in memory whose rows are `row_pitch` apart and
 * slices `slice_pitch` apart: from where it starts to the end of its last
 * slice's last row, each row `line` bytes. Every API's transfers measure a
 * pitched region by this - Direct3D's boxes of a texel layout's elements,
 * OpenCL's rects and image regions, OpenGL's unpacked pixels - on the guest
 * before it reads an application's memory and on the host before it reads a
 * guest's, so the two sides take the same regions. What each API refuses
 * besides (OpenGL's alignment and skips, OpenCL's zero pitch meaning tight
 * and its slice of whole rows, Direct3D's block elements) stays its own.
 * Every product and sum is checked: a size from outside never wraps. */
#ifndef AV_PITCHED_EXTENT_HPP
#define AV_PITCHED_EXTENT_HPP

#include <stdint.h>

namespace av {

class PitchedExtent {
public:
    /* `slices` slices of `rows` rows of `line` bytes each. */
    PitchedExtent(uint64_t line, uint64_t rows, uint64_t slices, uint64_t row_pitch, uint64_t slice_pitch);

    /* A box of no bytes: no row, no slice or an empty row. */
    bool empty() const;
    /* Whether the pitches step over what they must: rows - when there is
     * more than one - a whole line apart, and slices a whole slice of rows,
     * its last row's line included. Not more: a slice need not be padded to
     * a whole row pitch after its last row. */
    bool pitches_hold() const;
    /* Where row `row` of slice `slice` starts; false when that overflows. */
    [[nodiscard]] bool offset_of(uint64_t row, uint64_t slice, uint64_t &offset) const;
    /* One past the box's last byte when it starts `start` bytes in; false
     * for an empty box or when that overflows. */
    [[nodiscard]] bool end(uint64_t start, uint64_t &end) const;
    /* Whether a box starting at 0 lies within `size` bytes. */
    bool within(uint64_t size) const;
    /* Whether it lies within `size` bytes with pitches that hold it. */
    bool fits(uint64_t size) const;

private:
    uint64_t line_;
    uint64_t rows_;
    uint64_t slices_;
    uint64_t row_pitch_;
    uint64_t slice_pitch_;
};

}  // namespace av

#endif  // AV_PITCHED_EXTENT_HPP
