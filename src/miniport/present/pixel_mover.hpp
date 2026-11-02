#ifndef AV_PIXEL_MOVER_HPP
#define AV_PIXEL_MOVER_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PresentRegion;
class PresentSurface;

class PixelMover {
public:
    [[nodiscard]] virtual Status copy(const PresentSurface &source, const PresentSurface &destination, const PresentRegion *regions, uint32_t count) = 0;
    [[nodiscard]] virtual Status fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions, uint32_t count) = 0;

protected:
    ~PixelMover() = default;
};

}

#endif
