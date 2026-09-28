/* A present surface's pages mapped into kernel space for one blit, as one view
 * (Platform::map_pages): a row is then one copy, where mapping the page each
 * piece of it is in, and unmapping it again, cost a TLB flush on every
 * processor per piece - most of a frame's time for a 1280x800 blt. Unmapped
 * when this goes. */
#ifndef AV_SURFACE_VIEW_HPP
#define AV_SURFACE_VIEW_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Platform;
class PresentSurface;

class SurfaceView {
public:
    SurfaceView(Platform &platform, const PresentSurface &surface);
    ~SurfaceView();
    SurfaceView(const SurfaceView &) = delete;
    SurfaceView &operator=(const SurfaceView &) = delete;

    /* not_found when the platform cannot map the surface's pages. */
    [[nodiscard]] Status map();
    /* The pixel at column `x` of row `y`, where the surface's layout puts it;
     * the view is mapped, and the pixel is inside the surface. */
    uint8_t *pixel(uint32_t x, uint32_t y) const;

private:
    Platform             &platform_;
    const PresentSurface &surface_;
    uint8_t              *view_;
    void                 *mapping_;
};

}  // namespace av

#endif  // AV_SURFACE_VIEW_HPP
