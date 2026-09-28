#include "proto/resources/surface_layout.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SurfaceLayout::SurfaceLayout() : width_(0u), height_(0u), format_(0u), pitch_(0u)
{
    AV_SCOPE("SurfaceLayout::SurfaceLayout");
}

SurfaceLayout::SurfaceLayout(uint32_t width, uint32_t height, uint32_t format, uint32_t pitch)
    : width_(width), height_(height), format_(format), pitch_(pitch)
    {
        AV_SCOPE("SurfaceLayout::SurfaceLayout");
    }

uint32_t SurfaceLayout::width() const
{
    AV_SCOPE("SurfaceLayout::width");
    return width_;
}
uint32_t SurfaceLayout::height() const
{
    AV_SCOPE("SurfaceLayout::height");
    return height_;
}
uint32_t SurfaceLayout::format() const
{
    AV_SCOPE("SurfaceLayout::format");
    return format_;
}
uint32_t SurfaceLayout::pitch() const
{
    AV_SCOPE("SurfaceLayout::pitch");
    return pitch_;
}
uint64_t SurfaceLayout::bytes() const
{
    AV_SCOPE("SurfaceLayout::bytes");
    return uint64_t{pitch_} * height_;
}
bool     SurfaceLayout::is_surface() const
{
    AV_SCOPE("SurfaceLayout::is_surface");
    return width_ != 0u && height_ != 0u && pitch_ != 0u;
}

bool SurfaceLayout::red_first() const
{
    AV_SCOPE("SurfaceLayout::red_first");
    constexpr uint32_t a8b8g8r8 = 32u;   /* D3DDDIFMT_A8B8G8R8 */
    constexpr uint32_t x8b8g8r8 = 33u;   /* D3DDDIFMT_X8B8G8R8 */
    return format_ == a8b8g8r8 || format_ == x8b8g8r8;
}

}  // namespace av
