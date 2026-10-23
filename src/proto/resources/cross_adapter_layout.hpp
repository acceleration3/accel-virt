/* How dxgkrnl lays out a cross-adapter allocation (D3DKMDT_GDISURFACETYPE
 * TEXTURE_CROSSADAPTER, which is also what a Direct3D 12 shared-cross-adapter
 * heap arrives as): a texture's rows are as long as its row's bytes rounded up
 * to D3DKMT_CROSS_ADAPTER_RESOURCE_PITCH_ALIGNMENT and it has as many of them
 * as its height rounded up to D3DKMT_CROSS_ADAPTER_RESOURCE_HEIGHT_ALIGNMENT -
 * dxgkrnl refuses a pitch from the driver that is not the first. The one
 * answer both ends compute: the miniport's descriptor for a resource another
 * adapter's device made, and the user-mode driver's for the one it makes
 * itself, which must agree on the pages. A heap is the shape with no format:
 * `width` bytes, one row. */
#ifndef AV_CROSS_ADAPTER_LAYOUT_HPP
#define AV_CROSS_ADAPTER_LAYOUT_HPP

#include "proto/resources/surface_layout.hpp"

#include <stdint.h>

namespace av {

class CrossAdapterLayout {
public:
    static constexpr uint32_t pitch_alignment = 128u;
    static constexpr uint32_t height_alignment = 4u;

    /* `width` by `height` texels of the D3DDDIFORMAT `format`, each
     * `bytes_per_pixel` bytes; a `format` of 0 and a `height` of 1 is a heap
     * of `width` bytes. */
    CrossAdapterLayout(uint32_t width, uint32_t height, uint32_t format, uint32_t bytes_per_pixel);

    /* False for no texels, a heap taller than a row, or sizes that do not fit
     * 32 bits of pitch or bytes (what basicrender.sys refuses too). */
    bool     valid() const;
    uint32_t pitch() const;
    /* The pages the allocation takes: pitch times the rounded rows. */
    uint64_t bytes() const;
    /* The surface as the descriptor carries it: the real height, whose bytes
     * fit in bytes(). */
    SurfaceLayout surface() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t format_;
    uint32_t pitch_;
    uint64_t bytes_;
    bool     valid_;
};

}  // namespace av

#endif  // AV_CROSS_ADAPTER_LAYOUT_HPP
