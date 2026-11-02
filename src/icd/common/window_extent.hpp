#ifndef AV_ICD_WINDOW_EXTENT_HPP
#define AV_ICD_WINDOW_EXTENT_HPP

#include <stdint.h>

namespace av::icd {

class WindowSurface;

class WindowExtent {
public:
    enum class Look {
        gone,
        empty,
        resized,
        same,
    };

    WindowExtent(WindowSurface &surface, void *window);
    WindowExtent(WindowSurface &surface, void *window, uint32_t width, uint32_t height);
    WindowExtent(const WindowExtent &) = delete;
    WindowExtent &operator=(const WindowExtent &) = delete;

    Look     look();
    uint32_t width() const;
    uint32_t height() const;

private:
    WindowSurface &surface_;
    void          *window_;
    uint32_t       held_width_;
    uint32_t       held_height_;
    uint32_t       width_;
    uint32_t       height_;
};

}

#endif
