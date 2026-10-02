/* What moves a present's pixels once PresentBlit has clipped them into
 * regions: the host, through its own mapping of guest RAM (PresentCopier), or
 * the miniport through a view of the pages it maps itself (MappedPixels). Each
 * region is inside both surfaces, and the surfaces are reachable. */
#ifndef AV_PIXEL_MOVER_HPP
#define AV_PIXEL_MOVER_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PresentRegion;
class PresentSurface;

class PixelMover {
public:
    /* Each region of `source` into the same size of `destination`. */
    [[nodiscard]] virtual Status copy(const PresentSurface &source, const PresentSurface &destination, const PresentRegion *regions, uint32_t count) = 0;
    /* `colour` over each region of `destination`. */
    [[nodiscard]] virtual Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions, uint32_t count) = 0;

protected:
    ~PixelMover() = default;
};

}  // namespace av

#endif  // AV_PIXEL_MOVER_HPP
