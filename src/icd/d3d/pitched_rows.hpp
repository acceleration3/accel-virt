/* Rows of pixels in memory at a pitch, as many as fit `bytes`: a surface the
 * host read back, an allocation's locked pages, a lock's shadow. Copying
 * between two takes each row's narrower pitch, and stops at whichever runs
 * out first. */
#ifndef AV_D3D_PITCHED_ROWS_HPP
#define AV_D3D_PITCHED_ROWS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class PitchedRows {
public:
    PitchedRows(uint8_t *base, uint32_t pitch, size_t bytes);

    /* The first `rows` rows of `from` copied over these; how many were. */
    uint32_t copy_from(const PitchedRows &from, uint32_t rows);

private:
    /* Whether row `row` lies whole inside the memory. */
    bool holds(uint32_t row) const;

    uint8_t *base_;
    uint32_t pitch_;
    size_t   bytes_;
};

}  // namespace av::d3d

#endif  // AV_D3D_PITCHED_ROWS_HPP
