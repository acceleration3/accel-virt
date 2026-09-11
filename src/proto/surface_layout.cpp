#include "surface_layout.hpp"

namespace av {

SurfaceLayout::SurfaceLayout() : width_(0u), height_(0u), format_(0u), pitch_(0u) {}

SurfaceLayout::SurfaceLayout(uint32_t width, uint32_t height, uint32_t format, uint32_t pitch)
    : width_(width), height_(height), format_(format), pitch_(pitch) {}

uint32_t SurfaceLayout::width() const { return width_; }
uint32_t SurfaceLayout::height() const { return height_; }
uint32_t SurfaceLayout::format() const { return format_; }
uint32_t SurfaceLayout::pitch() const { return pitch_; }
uint64_t SurfaceLayout::bytes() const { return uint64_t{pitch_} * height_; }
bool     SurfaceLayout::is_surface() const { return width_ != 0u && height_ != 0u && pitch_ != 0u; }

}  // namespace av
