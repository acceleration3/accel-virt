#ifndef AV_CLICD_CL_STAGED_ROWS_HPP
#define AV_CLICD_CL_STAGED_ROWS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::icd {
class RowPieces;
}

namespace av::clicd {

class ClImageRegion;

class ClStagedRows {
public:
    ClStagedRows(const ClImageRegion &layout, const icd::RowPieces &piece, size_t slice);

    void pack(const uint8_t *application, uint8_t *staged) const;
    void unpack(const uint8_t *staged, uint8_t *application) const;

private:
    size_t   row(size_t index) const;
    uint64_t spread() const;

    const ClImageRegion &layout_;
    size_t               slice_;
    size_t               top_;
    size_t               rows_;
    size_t               column_;
    size_t               row_bytes_;
};

}

#endif
