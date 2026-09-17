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

/* The last frame the ICD blitted, which is what a window would be showing. */
bool SurfaceWindowFrame::read(void *window, std::uint32_t width, std::uint32_t height, std::uint8_t *bgra)
{
    if (window != window_ || width != windows_.last_blit_width() || height != windows_.last_blit_height())
        return false;
    const std::uint32_t bytes = width * height * 4u;
    if (bytes > ScriptedWindowSurface::kept_bytes) return false;
    std::memcpy(bgra, windows_.last_blit_bytes(), bytes);
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

}  // namespace av
