#ifndef AV_D3D9_TEXTURE_WRITE_HPP
#define AV_D3D9_TEXTURE_WRITE_HPP

#include "proto/generated/d3d9/d3d9_wire_types.h"

#include <stdint.h>

namespace av {

class TexelLayout;

class D3d9TextureWrite {
public:
    D3d9TextureWrite(const d3d9proto::Box &box, uint32_t row_pitch, uint32_t slice_pitch, uint64_t data_size,
                     const TexelLayout &layout);

    bool     fits(uint32_t width, uint32_t height, uint32_t depth) const;
    uint32_t rows() const;
    uint32_t row_bytes() const;
    uint32_t slices() const;

private:
    bool spans(uint32_t first, uint32_t last, uint32_t extent, uint32_t element) const;

    d3d9proto::Box          box_;
    uint32_t                row_pitch_;
    uint32_t                slice_pitch_;
    uint64_t                data_size_;
    const TexelLayout      &layout_;
};

}

#endif
