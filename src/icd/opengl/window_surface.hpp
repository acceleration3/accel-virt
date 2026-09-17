/* The windows an OpenGL application draws on, as the ICD sees them (feature
 * 050, research.md §6): the window behind a device context, its client size,
 * and a finished frame put on the DC. A DC and a window are opaque pointers the
 * ICD only compares and passes back. The Windows shim answers through user32
 * and gdi32; a test declares its windows. */
#ifndef AV_GLICD_WINDOW_SURFACE_HPP
#define AV_GLICD_WINDOW_SURFACE_HPP

#include <stdint.h>

namespace av::glicd {

class WindowSurface {
public:
    WindowSurface(const WindowSurface &) = delete;
    WindowSurface &operator=(const WindowSurface &) = delete;

    /* The window `dc` draws on; null for a DC with none, such as a memory DC. */
    virtual void *window_of(void *dc) = 0;
    /* false for a window that no longer exists. */
    virtual bool  client_size(void *window, uint32_t &width, uint32_t &height) = 0;
    /* `width` x `height` BGRA pixels, the bottom row first, drawn at the DC's
     * origin; false when the DC refused them. */
    virtual bool  blit(void *dc, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up) = 0;

protected:
    WindowSurface() = default;
    ~WindowSurface() = default;
};

}  // namespace av::glicd

#endif  // AV_GLICD_WINDOW_SURFACE_HPP
