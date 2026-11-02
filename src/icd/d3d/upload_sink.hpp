#ifndef AV_D3D_UPLOAD_SINK_HPP
#define AV_D3D_UPLOAD_SINK_HPP

#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

namespace av::d3d {

class UploadSink {
public:
    [[nodiscard]] virtual bool piece(const d3dproto::Box &box, const uint8_t *data, uint64_t bytes, uint32_t row_pitch, uint32_t slice_pitch, bool first) = 0;

protected:
    UploadSink() = default;
    ~UploadSink() = default;
};

}

#endif
