#ifndef AV_ICD_WINDOW_SURFACE_HPP
#define AV_ICD_WINDOW_SURFACE_HPP

#include <stdint.h>

namespace av::icd {

class WindowSurface {
public:
    WindowSurface(const WindowSurface &) = delete;
    WindowSurface &operator=(const WindowSurface &) = delete;

    virtual void *window_of(void *dc) = 0;
    virtual bool  client_size(void *window, uint32_t &width, uint32_t &height) = 0;
    virtual bool  blit(void *dc, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up) = 0;
    virtual bool  blit_window(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up) = 0;
    virtual bool  blit_window_top_down(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_top_down) = 0;

protected:
    WindowSurface() = default;
    ~WindowSurface() = default;
};

}

#endif
