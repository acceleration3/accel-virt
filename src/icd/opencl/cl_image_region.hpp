#ifndef AV_CLICD_CL_IMAGE_REGION_HPP
#define AV_CLICD_CL_IMAGE_REGION_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class ClImageRegion {
public:
    ClImageRegion(const size_t *origin, const size_t *region, size_t row_pitch,
                  size_t slice_pitch, size_t element_bytes);

    bool   valid() const;
    size_t width() const;
    size_t rows() const;
    size_t slices() const;
    size_t element_bytes() const;
    size_t row_bytes() const;
    size_t row_pitch() const;
    size_t slice_pitch() const;
    bool   rows_packed() const;
    size_t row_offset(size_t row, size_t slice) const;
    void   origin_of(size_t x, size_t row, size_t slice, size_t *into) const;
    [[nodiscard]] bool offset_of(const size_t *point, size_t &offset) const;
    bool   slices_are_whole_rows() const;

private:
    size_t origin_[3];
    size_t region_[3];
    size_t row_pitch_;
    size_t slice_pitch_;
    size_t element_bytes_;
};

}

#endif
