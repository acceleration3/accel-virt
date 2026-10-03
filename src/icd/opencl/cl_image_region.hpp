/* The part of an image one transfer names, and how the application's memory
 * is laid out for it.
 *
 * clEnqueueReadImage and clEnqueueWriteImage name an origin and a region in
 * texels, and a row and slice pitch in bytes for the application's side, where
 * a pitch of zero means rows packed tight. Cutting such a transfer into
 * commands that fit is a matter of rows and slices, so that is what this
 * answers: how long a row is, where row y of slice z starts in the
 * application's memory, and the region a run of rows or a piece of one row is.
 * The element size is the image's own, asked of the host rather than worked
 * out here from a format table the host already has. */
#ifndef AV_CLICD_CL_IMAGE_REGION_HPP
#define AV_CLICD_CL_IMAGE_REGION_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class ClImageRegion {
public:
    /* `origin` and `region` are the application's three texel coordinates;
     * the pitches are its own, zero for tight. */
    ClImageRegion(const size_t *origin, const size_t *region, size_t row_pitch,
                  size_t slice_pitch, size_t element_bytes);

    /* A region the API would refuse: no element size, an empty extent, or a
     * pitch smaller than what it has to hold. Refused before anything is sent. */
    bool   valid() const;
    size_t width() const;
    size_t rows() const;
    size_t slices() const;
    size_t element_bytes() const;
    /* One row of the region, packed. */
    size_t row_bytes() const;
    size_t row_pitch() const;
    size_t slice_pitch() const;
    /* Whether the application's rows follow one another with no gap, so that a
     * run of them is one span of its memory. */
    bool   rows_packed() const;
    /* Where row `row` of slice `slice` starts, from the application's pointer. */
    size_t row_offset(size_t row, size_t slice) const;
    /* The image's own coordinates of texel `x` of that row. */
    void   origin_of(size_t x, size_t row, size_t slice, size_t *into) const;
    /* Where `point` - an element, a row and a slice - lies in this layout,
     * from where the layout starts: false where that does not fit a size_t. */
    [[nodiscard]] bool offset_of(const size_t *point, size_t &offset) const;
    /* A buffer rectangle's rule for a pitch the application named: a slice is
     * a whole number of rows. */
    bool   slices_are_whole_rows() const;

private:
    size_t origin_[3];
    size_t region_[3];
    size_t row_pitch_;
    size_t slice_pitch_;
    size_t element_bytes_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_IMAGE_REGION_HPP
