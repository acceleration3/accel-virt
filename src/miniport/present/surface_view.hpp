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

    [[nodiscard]] Status map();
    uint8_t *pixel(uint32_t x, uint32_t y) const;

private:
    Platform             &platform_;
    const PresentSurface &surface_;
    uint8_t              *view_;
    void                 *mapping_;
};

}

#endif
