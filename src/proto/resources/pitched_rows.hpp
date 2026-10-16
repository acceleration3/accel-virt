/* Rows of bytes in memory at a pitch, as many as lie whole within `bytes`: a
 * surface the host read back, an allocation's locked pages, a lock's shadow,
 * an application's region and the packed block it is staged in. The one row
 * copier every API's transfers use: a copy between two stops at whichever
 * runs out of whole rows first, and never reads or writes past either. */
#ifndef AV_PITCHED_ROWS_HPP
#define AV_PITCHED_ROWS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class PitchedRows {
public:
    PitchedRows(uint8_t *base, uint64_t pitch, uint64_t bytes);
    PitchedRows(const uint8_t *base, uint64_t pitch, uint64_t bytes);

    /* The first `rows` rows of `from` copied over these, each row the
     * narrower of the two pitches long; how many were. */
    uint32_t copy_from(const PitchedRows &from, uint32_t rows);
    /* The same, each row `line` bytes long; how many were. */
    uint32_t copy_lines(const PitchedRows &from, uint32_t rows, uint64_t line);

private:
    /* Whether `line` bytes from row `row`'s start lie inside the memory. */
    bool holds(uint32_t row, uint64_t line) const;

    uint8_t       *base_;
    const uint8_t *source_;
    uint64_t       pitch_;
    uint64_t       bytes_;
};

}  // namespace av

#endif  // AV_PITCHED_ROWS_HPP
