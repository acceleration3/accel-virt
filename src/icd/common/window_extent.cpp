#include "icd/common/window_extent.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/window_surface.hpp"

namespace av::icd {

WindowExtent::WindowExtent(WindowSurface &surface, void *window) : WindowExtent(surface, window, 0u, 0u)
{
    AV_SCOPE("WindowExtent::WindowExtent");
}

WindowExtent::WindowExtent(WindowSurface &surface, void *window, uint32_t width, uint32_t height)
    : surface_(surface), window_(window), held_width_(width), held_height_(height), width_(0u), height_(0u)
{
    AV_SCOPE("WindowExtent::WindowExtent");
}

WindowExtent::Look WindowExtent::look()
{
    AV_SCOPE("WindowExtent::look");
    width_ = 0u;
    height_ = 0u;
    if (window_ == nullptr || !surface_.client_size(window_, width_, height_)) {
        width_ = 0u;
        height_ = 0u;
        return Look::gone;
    }
    if (width_ == 0u || height_ == 0u) return Look::empty;
    return width_ == held_width_ && height_ == held_height_ ? Look::same : Look::resized;
}

uint32_t WindowExtent::width() const
{
    AV_SCOPE("WindowExtent::width");
    return width_;
}

uint32_t WindowExtent::height() const
{
    AV_SCOPE("WindowExtent::height");
    return height_;
}

}
