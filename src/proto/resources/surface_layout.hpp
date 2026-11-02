#ifndef AV_SURFACE_LAYOUT_HPP
#define AV_SURFACE_LAYOUT_HPP

#include <stdint.h>

namespace av {

class SurfaceLayout {
public:
    SurfaceLayout();
    SurfaceLayout(uint32_t width, uint32_t height, uint32_t format, uint32_t pitch);

    uint32_t width() const;
    uint32_t height() const;
    uint32_t format() const;
    uint32_t pitch() const;
    uint64_t bytes() const;
    bool     is_surface() const;
    bool     red_first() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t format_;
    uint32_t pitch_;
};

}

#endif
