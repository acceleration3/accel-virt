#ifndef AV_ICD_ROW_PIECES_HPP
#define AV_ICD_ROW_PIECES_HPP

#include "icd/common/split_report.hpp"
#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class RowPieces {
public:
    RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most);
    RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most,
              ClientTransport &wire);

    [[nodiscard]] bool next();
    uint32_t top() const;
    uint32_t bottom() const;
    uint32_t left() const;
    uint32_t right() const;
    uint64_t offset() const;
    uint64_t bytes() const;
    bool     first() const;

private:
    RowPieces(const TexelLayout &layout, uint32_t width, uint32_t height, uint32_t pitch, uint64_t most,
              const SplitReport &report);

    TexelLayout layout_;
    uint32_t    width_;
    uint32_t    height_;
    uint32_t    pitch_;
    uint32_t    rows_;
    uint32_t    columns_;
    uint32_t    rows_per_piece_;
    uint32_t    columns_per_piece_;
    uint32_t    row_;
    uint32_t    column_;
    uint32_t    row_count_;
    uint32_t    column_count_;
    SplitReport report_;
};

}

#endif
