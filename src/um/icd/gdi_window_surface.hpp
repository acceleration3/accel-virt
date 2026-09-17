/* An ICD's windows through user32 and gdi32: the window behind a DC, its
 * client size, and a frame put bottom-up on a DC or on the window's own DC.
 * Translation only. */
#ifndef AV_UM_GDI_WINDOW_SURFACE_HPP
#define AV_UM_GDI_WINDOW_SURFACE_HPP

#include "icd/common/window_surface.hpp"

#include <stdint.h>

namespace av {

class GdiWindowSurface final : public icd::WindowSurface {
public:
    constexpr GdiWindowSurface() = default;

    void *window_of(void *dc) override;
    bool  client_size(void *window, uint32_t &width, uint32_t &height) override;
    bool  blit(void *dc, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up) override;
    bool  blit_window(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up) override;

private:
    /* A frame is 32-bit BGRA. */
    static constexpr uint16_t bits_per_pixel = 32u;

    uint16_t bits_ = bits_per_pixel;
};

}  // namespace av

#endif  // AV_UM_GDI_WINDOW_SURFACE_HPP
