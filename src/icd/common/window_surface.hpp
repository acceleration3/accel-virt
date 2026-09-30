/* The windows an application draws on, as an ICD sees them (features 050 and
 * 046): the window behind a device context, its client size, and a finished
 * frame put on a DC or on the window itself. A DC and a window are opaque
 * pointers the ICD only compares and passes back. The Windows shim answers
 * through user32 and gdi32; a test declares its windows. */
#ifndef AV_ICD_WINDOW_SURFACE_HPP
#define AV_ICD_WINDOW_SURFACE_HPP

#include <stdint.h>

namespace av::icd {

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
    /* The same on `window`'s own client area, for a caller that holds no DC:
     * OpenGL's present thread. false for a window that no longer exists. */
    virtual bool  blit_window(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up) = 0;
    /* The same with the top row first, as a Vulkan image holds its rows: a
     * swapchain's read-back, drawn as it is. */
    virtual bool  blit_window_top_down(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_top_down) = 0;

protected:
    WindowSurface() = default;
    ~WindowSurface() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_WINDOW_SURFACE_HPP
