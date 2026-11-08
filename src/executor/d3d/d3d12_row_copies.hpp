#ifndef AV_D3D12_ROW_COPIES_HPP
#define AV_D3D12_ROW_COPIES_HPP

#include "proto/d3d/dxgi_format_layout.hpp"

#include <stdint.h>

namespace av {

class D3d12RowCopies {
public:
    D3d12RowCopies(uint32_t format, uint32_t footprint_height, uint32_t row_pitch);

    bool     needed() const;
    uint64_t row_offset(uint64_t base, uint32_t slice, uint32_t row, uint32_t left) const;
    uint32_t pitch_of(uint32_t width) const;

private:
    DxgiFormatLayout layout_;
    uint32_t         height_;
    uint32_t         row_pitch_;
};

}

#endif
