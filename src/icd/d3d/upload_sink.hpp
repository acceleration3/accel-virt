/* What one Direct3D runtime sends a piece of an upload as (TextureUpload):
 * Direct3D 9's WriteTexture9, Direct3D 11's UpdateSubresource. */
#ifndef AV_D3D_UPLOAD_SINK_HPP
#define AV_D3D_UPLOAD_SINK_HPP

#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

namespace av::d3d {

class UploadSink {
public:
    /* `box` of the subresource, its `bytes` at `data` with rows `row_pitch`
     * and slices `slice_pitch` apart; `first` is the upload's first piece,
     * the only one that may discard what the subresource held. False stops
     * the upload. */
    [[nodiscard]] virtual bool piece(const d3dproto::Box &box, const uint8_t *data, uint64_t bytes, uint32_t row_pitch, uint32_t slice_pitch, bool first) = 0;

protected:
    UploadSink() = default;
    /* Non-virtual: nothing is destroyed through this pointer (CLAUDE.md,
     * Adaptations). */
    ~UploadSink() = default;
};

}  // namespace av::d3d

#endif  // AV_D3D_UPLOAD_SINK_HPP
