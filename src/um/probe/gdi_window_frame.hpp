#ifndef AV_UM_GDI_WINDOW_FRAME_HPP
#define AV_UM_GDI_WINDOW_FRAME_HPP

#include "guest/probe/window_frame.hpp"

#include <stdint.h>

namespace av {

class GdiWindowFrame final : public WindowFrame {
public:
    constexpr GdiWindowFrame() = default;

    bool size(void *window, uint32_t &width, uint32_t &height) override;
    bool read(void *window, uint32_t width, uint32_t height, uint8_t *bgra) override;
    bool resize(void *window, uint32_t width, uint32_t height) override;

private:
    static constexpr uint16_t bits_per_pixel = 32u;

    uint16_t bits_ = bits_per_pixel;
};

}

#endif
