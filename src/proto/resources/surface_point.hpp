/* A host surface as one present left it: the shared id the miniport minted
 * over the surface's kernel allocation, and the point that present published
 * on it - the surface's nth present (docs/shared-surfaces.md, "Points"). What
 * a blt's source is named by from the miniport to the host's staged frames. */
#ifndef AV_SURFACE_POINT_HPP
#define AV_SURFACE_POINT_HPP

#include <stdint.h>

namespace av {

class SurfacePoint {
public:
    SurfacePoint();
    SurfacePoint(uint64_t surface, uint32_t point);

    uint64_t surface() const;
    uint32_t point() const;
    /* Whether this names a frame: a surface and a point, neither 0. */
    bool     names() const;
    bool     operator==(const SurfacePoint &other) const;

private:
    uint64_t surface_;
    uint32_t point_;
};

}  // namespace av

#endif  // AV_SURFACE_POINT_HPP
