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
    bool  blit_window_top_down(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_top_down) override;

private:
    bool draw_on_window(void *window, uint32_t width, int32_t dib_height, const uint8_t *rows);
    bool draw(void *dc, uint32_t width, int32_t dib_height, const uint8_t *rows);

    static constexpr uint16_t bits_per_pixel = 32u;

    uint16_t bits_ = bits_per_pixel;
};

}

#endif
