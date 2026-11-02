#ifndef AV_FORMAT_CAPS_ROWS_HPP
#define AV_FORMAT_CAPS_ROWS_HPP

#include "proto/d3d/dxgi_format.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

namespace av {

class FormatCapsRows {
public:
    FormatCapsRows();

    void     answer(uint32_t format, const d3dproto::FormatCaps &row);
    uint64_t give(uint8_t *into, uint64_t capacity) const;

private:
    d3dproto::FormatCaps rows_[DxgiFormat::count];
};

}

#endif
