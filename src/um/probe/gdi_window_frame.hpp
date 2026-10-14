/* The probe's window through user32 and gdi32 (feature 046): its client size,
 * its pixels as the desktop composes them, read into a DIB section, and a
 * resize. Reading the window is the probe's proof that a present reached it -
 * the pixels come from the desktop, not from the driver. Translation only. */
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
    /* A frame is 32-bit BGRA, as the driver's blit writes it. */
    static constexpr uint16_t bits_per_pixel = 32u;

    uint16_t bits_ = bits_per_pixel;
};

}  // namespace av

#endif  // AV_UM_GDI_WINDOW_FRAME_HPP
