/* One piece of a transfer staged in an arena block (ClTransferChunks): where
 * its rows lie in the application's memory, at the application's pitches, and
 * where they lie in the block, packed tight from its start - which is how the
 * host reads and writes a region it is given no pitch for. */
#ifndef AV_CLICD_CL_STAGED_ROWS_HPP
#define AV_CLICD_CL_STAGED_ROWS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::icd {
class RowPieces;
}  // namespace av::icd

namespace av::clicd {

class ClImageRegion;

class ClStagedRows {
public:
    /* The piece `piece` has reached of slice `slice` of `layout`. */
    ClStagedRows(const ClImageRegion &layout, const icd::RowPieces &piece, size_t slice);

    /* The application's rows, from where `layout` starts at `application`,
     * into the block at `staged`, packed. */
    void pack(const uint8_t *application, uint8_t *staged) const;
    /* The block's packed rows out to the application's. */
    void unpack(const uint8_t *staged, uint8_t *application) const;

private:
    /* Where the piece's `index`th row starts from where the layout does. */
    size_t   row(size_t index) const;
    uint64_t spread() const;

    const ClImageRegion &layout_;
    size_t               slice_;
    size_t               top_;
    size_t               rows_;
    size_t               column_;
    size_t               row_bytes_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_STAGED_ROWS_HPP
