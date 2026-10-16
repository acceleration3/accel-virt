/* Every DXGI format as one host device answered it, a d3dproto::FormatCaps
 * row each: what Direct3D 11's QueryCaps11 and Direct3D 12's QueryCaps12 hand
 * the guest in one reply, from DXGI_FORMAT 0 up, as many whole rows as the
 * guest made room for. Measured once when the device is made, by
 * D3d11Capabilities and D3d12Capabilities, which ask their own runtime. */
#ifndef AV_FORMAT_CAPS_ROWS_HPP
#define AV_FORMAT_CAPS_ROWS_HPP

#include "proto/d3d/dxgi_format.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

namespace av {

class FormatCapsRows {
public:
    FormatCapsRows();

    /* `format`'s row; a value past the table is no format and is ignored. */
    void     answer(uint32_t format, const d3dproto::FormatCaps &row);
    /* The rows into `into`, as many whole ones as `capacity` bytes hold: the
     * bytes written. */
    uint64_t give(uint8_t *into, uint64_t capacity) const;

private:
    d3dproto::FormatCaps rows_[DxgiFormat::count];
};

}  // namespace av

#endif  // AV_FORMAT_CAPS_ROWS_HPP
