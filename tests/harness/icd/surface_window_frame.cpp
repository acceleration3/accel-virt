#include "harness/icd/surface_window_frame.hpp"

#include "harness/icd/scripted_window_surface.hpp"

#include <cstring>

namespace av {

SurfaceWindowFrame::SurfaceWindowFrame(ScriptedWindowSurface &windows, void *window, std::uint32_t width,
                                       std::uint32_t height)
    : windows_(windows), window_(window), width_(width), height_(height)
{
}

bool SurfaceWindowFrame::size(void *window, std::uint32_t &width, std::uint32_t &height)
{
    if (window != window_) return false;
    width = width_;
    height = height_;
    return true;
}

bool SurfaceWindowFrame::read(void *window, std::uint32_t width, std::uint32_t height, std::uint8_t *bgra)
{
    if (window != window_ || width != windows_.last_blit_width() || height != windows_.last_blit_height())
        return false;
    const std::uint32_t row = width * 4u;
    if (row * height > ScriptedWindowSurface::kept_bytes) return false;
    const bool top_down = windows_.last_blit_top_down();
    for (std::uint32_t line = 0u; line < height; line++)
        std::memcpy(bgra + line * row, windows_.last_blit_bytes() + (top_down ? height - 1u - line : line) * row, row);
    return true;
}

bool SurfaceWindowFrame::resize(void *window, std::uint32_t width, std::uint32_t height)
{
    if (window != window_) return false;
    windows_.resize(window, width, height);
    width_ = width;
    height_ = height;
    return true;
}

}
