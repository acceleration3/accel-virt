/* The probe's window, as the probe sees it from outside the driver (feature
 * 046): its client size, its pixels read back, and a resize. Two things
 * implement it: GdiWindowFrame in the guest, over user32 and gdi32, and the
 * scripted window the probe's unit tests and the full-stack rig play, so it is
 * virtual for the reason Platform is. Reading the window is what proves a
 * present reached it: nothing inside the driver is asked. */
#ifndef AV_WINDOW_FRAME_HPP
#define AV_WINDOW_FRAME_HPP

#include <stdint.h>

namespace av {

class WindowFrame {
public:
    WindowFrame(const WindowFrame &) = delete;
    WindowFrame &operator=(const WindowFrame &) = delete;

    virtual bool size(void *window, uint32_t &width, uint32_t &height) = 0;
    /* `width` x `height` pixels of the window's client area into `bgra`,
     * bottom row first, as a DIB holds them. */
    virtual bool read(void *window, uint32_t width, uint32_t height, uint8_t *bgra) = 0;
    /* The client area becomes `width` x `height`. */
    virtual bool resize(void *window, uint32_t width, uint32_t height) = 0;

protected:
    WindowFrame() = default;
    ~WindowFrame() = default;
};

}  // namespace av

#endif  // AV_WINDOW_FRAME_HPP
