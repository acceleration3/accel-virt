/* The pixel layout of a surface: its dimensions, D3DDDIFORMAT and the bytes per
 * row. Zero everywhere for an allocation that is not a surface. */
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
    /* pitch * height, which cannot overflow 64 bits. */
    uint64_t bytes() const;
    bool     is_surface() const;
    /* Whether a pixel's bytes run red first - A8B8G8R8 and X8B8G8R8, what a
     * Direct3D R8G8B8A8 surface is - rather than blue first like A8R8G8B8, the
     * desktop's: two surfaces that disagree swap red and blue when copied. */
    bool     red_first() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t format_;
    uint32_t pitch_;
};

}  // namespace av

#endif  // AV_SURFACE_LAYOUT_HPP
