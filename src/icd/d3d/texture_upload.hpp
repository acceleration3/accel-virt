#ifndef AV_D3D_TEXTURE_UPLOAD_HPP
#define AV_D3D_TEXTURE_UPLOAD_HPP

#include "proto/generated/d3d/d3d_wire_types.h"
#include "proto/resources/pitched_extent.hpp"
#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {
class SplitReport;
}

namespace av::d3d {

class UploadSink;

class TextureUpload {
public:
    TextureUpload(const TexelLayout &layout, const d3dproto::Box &box, uint32_t row_pitch, uint32_t slice_pitch,
                  uint64_t most, ClientTransport &wire);

    uint64_t bytes() const;
    bool     readable() const;
    [[nodiscard]] bool send(UploadSink &sink, const uint8_t *data) const;

private:
    PitchedExtent extent() const;
    uint32_t width() const;
    uint32_t height() const;
    uint32_t slices() const;
    uint64_t     piece_most() const;
    [[nodiscard]] bool send_slice(UploadSink &sink, const uint8_t *data, uint32_t slice, uint64_t most,
                                    icd::SplitReport &split) const;

    TexelLayout   layout_;
    d3dproto::Box box_;
    uint32_t      row_pitch_;
    uint32_t      slice_pitch_;
    uint64_t      most_;
    ClientTransport &wire_;
};

}

#endif
